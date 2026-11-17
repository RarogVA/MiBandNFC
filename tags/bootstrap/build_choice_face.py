#!/usr/bin/env python3
"""Build Roundabout RU NFC 5 or the NFC 6 installer face.

Default (--circle-app nfc): replaces the empty lower-circle Slot option with
the MiBandNFC "Метки" option; selecting it runs the bootstrap.

--circle-app findPhone builds the installer face: the empty option becomes a
Find Phone clone with the same Lua child. The first tap installs modules and
reboots; after that the overlay is transparent and the circle launches stock
Find Phone. Apps are launched from the menu.

No device I/O. Exact p67glt / 3.201.026 only.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tags/roundabout"))
sys.path.insert(0, str(ROOT / "tags/launcher"))
from pack_roundabout import (decode_frame, encode_frame, index_entries,  # noqa: E402
                             parse_image, rebuild_image, rgba)
from build_card import ASSET_NAMES  # noqa: E402

BASE_SHA = "b7875710957e54f4feee042ff1bcbc766f2fb49afc8cde19d4e047aa0e3bd3b2"
SOURCE_SHA = "ef28f70549660bc5f8a126c6e4dbcc53e13511d222863e405eade28b03766738"
FW_SHA = "66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40"
FACE_ID = "999999989"
FACE_NAME = "Roundabout RU NFC 5"
APP_ID = 0xD4
NORMAL = (0, 2, 3, 4)
SLOT_X, SLOT_Y = "126", "274"


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def entries(data):
    for theme in range(8):
        for kind in range(12):
            count, table = struct.unpack_from("<II", data, 168 + theme * 176 + 8 + kind * 8)
            require(count < 10000 and (not count or table + count * 16 <= len(data)),
                    "Invalid table bounds")
            for i in range(count):
                pos = table + i * 16
                uid, reserved, offset, size = struct.unpack_from("<IIII", data, pos)
                require(reserved == 0 and offset + size <= len(data), "Invalid resource bounds")
                require(uid >> 24 == kind, "Resource kind mismatch")
                yield theme, kind, uid, offset, size, pos


def file_resource(name, data):
    name = name.encode("ascii")
    require(0 < len(name) < 256 and len(data) < 1 << 24, "FileResource encoded length overflow")
    return struct.pack("<I", len(data) | (len(name) << 24)) + bytes(16) + name + data


def decode_file(payload):
    header = struct.unpack_from("<I", payload)[0]
    size, n = header & 0xFFFFFF, header >> 24
    require(payload[4:20] == bytes(16) and len(payload) == 20 + n + size, "Malformed FileResource")
    return payload[20:20 + n].decode("ascii"), payload[20 + n:]


def translations(text):
    text = text.encode("utf8")
    return struct.pack("<Q33I", (1 << 33) - 1, *([len(text)] * 33)) + text * 33


def build(args):
    app_id = 0x27 if args.circle_app == "findPhone" else APP_ID
    face_id = args.face_id
    face_name = args.face_name
    original = args.baseline.read_bytes()
    require(sha(original) == BASE_SHA, "Unexpected Roundabout RU baseline")
    source = args.source_zip.read_bytes()
    require(sha(source) == SOURCE_SHA, "Unexpected original source archive")
    firmware = args.firmware.read_bytes()
    require(sha(firmware) == FW_SHA, "Wrong firmware: addresses are version-specific")
    recorder = args.recorder.read_bytes()
    count, table = struct.unpack_from("<II", recorder, 168 + 8 + 5 * 8)
    require(count == 12, "Unexpected recorder FileResource table")
    rid, zero, offset, size = struct.unpack_from("<IIII", recorder, table)
    reference_name, reference_data = decode_file(recorder[offset:offset + size])
    require(rid == 0x05000000 and reference_name == "app/lua/main.lua",
            "Recorder Lua reference changed")
    require(file_resource(reference_name, reference_data) == recorder[offset:offset + size],
            "FileResource reference roundtrip failed")

    with zipfile.ZipFile(args.source_zip) as z:
        xml = ET.fromstring(z.read("resources/manifest.xml"))
        names = {k: int(v, 16) for k, v in
                 re.findall(r"^(\w+):\s*([0-9a-fA-F]+)",
                            z.read("uidmap.map").decode(), re.M)}
    resources = {n.get("name"): n for n in xml.find("Resources")}
    rev = {uid: name for name, uid in names.items()}

    file_payloads = [("app/lua/main.lua", args.lua.read_bytes()),
                     ("app/lua/catalog_core_v1.elf", args.core.read_bytes()),
                     ("app/lua/catalog_ui_v4.elf", args.ui.read_bytes())]
    for name, payload in file_payloads[1:]:
        require(payload[:4] == b"\x7fELF", "Not an ELF: " + name)
    for name in ASSET_NAMES:
        file_payloads.append(("app/lua/" + name + ".bin",
                              (ROOT / "tags/launcher/icons" / (name + ".bin")).read_bytes()))
    require(len({n for n, _ in file_payloads}) == len(file_payloads), "Duplicate embedded filename")

    original_entries = list(entries(original))
    old = {(t, u): (off, size, pos) for t, k, u, off, size, pos in original_entries}
    maxima = {kind: max(uid for _, k, uid, *_ in original_entries if k == kind)
              for kind in (2, 6, 9)}
    output = bytearray(original)
    allowed = set(range(0x28, 0x68)) | set(range(0x74, 0x7c))
    new_entries = {}
    changed_slots = {}
    original_slot_uids = []
    report_themes = []

    def append(payload):
        while len(output) % 4:
            output.append(0)
        at = len(output)
        output.extend(payload)
        return at, len(payload)

    def next_uid(kind):
        maxima[kind] += 1
        require(maxima[kind] >> 24 == kind, "Resource UID overflow")
        return maxima[kind]

    file_records = []
    for i, (name, payload) in enumerate(file_payloads):
        file_records.append((0x05000000 + i, 0, *append(file_resource(name, payload))))
    file_table, _ = append(b"".join(struct.pack("<IIII", *r) for r in file_records))

    editbox_uid = next_uid(2)
    editbox_size, editbox_pixels = rgba(Path(__file__).with_name("editbox-84.png"))
    require(editbox_size == (84, 84), "Editor boundary must equal live image bounds")
    editbox_payload = rebuild_image(2, editbox_size,
                                    [encode_frame(editbox_pixels, *editbox_size)])
    editbox_location = append(editbox_payload)

    for theme, node in enumerate(xml.findall("Theme")):
        layout = [n for n in node if n.get("x") == SLOT_X and n.get("y") == SLOT_Y]
        require(len(layout) == 1, "Expected one bottom circle per theme")
        slot_name = layout[0].get("ref")[1:]
        slot_uid = names[slot_name]
        slot_off, slot_size, slot_index_pos = old[theme, slot_uid]
        slot = original[slot_off:slot_off + slot_size]
        slot_count, reserved, slot_flags = struct.unpack_from("<BBH", slot)
        require(slot_count == 44 and reserved == 0 and slot_flags == 2 and len(slot) == 188,
                "Unexpected bottom Slot layout")
        require(slot[4 + slot_count * 4:] == bytes.fromhex("0400000033000000"),
                "Expected SlotGroupName string '3'")
        slot_uids = list(struct.unpack_from("<" + "I" * slot_count, slot, 4))
        original_slot_uids.append(slot_uids)
        last_uid = slot_uids[-1]
        empty_name = rev.get(last_uid)
        empty_node = resources.get(empty_name)
        require(empty_node is not None and not empty_node.get("jumpApp"),
                "Last Slot option is not the empty Widget")
        template_name = next(n.get("ref")[1:] for n in resources[slot_name]
                             if resources[n.get("ref")[1:]].get("jumpApp") == "findPhone")
        template_uid = names[template_name]
        template_off, template_size = old[theme, template_uid][:2]
        template = original[template_off:template_off + template_size]
        require(template_size == 64 and template[40] == 1,
                "Expected one-image Find Phone Widget template")
        template_flags = struct.unpack_from("<H", template, 42)[0]
        require((template_flags >> 4) & 255 == 0x27 and template_flags & 2,
                "Find Phone template flags changed")

        stock_widgets = []
        if theme in NORMAL:
            for index in range(slot_count - 1):
                old_uid = slot_uids[index]
                old_off, old_size = old[theme, old_uid][:2]
                stock = original[old_off:old_off + old_size]
                children = stock[40]
                require(old_size == 44 + 12 * children + 8,
                        "Unexpected stock Widget child layout")
                clone = bytearray(stock)
                at = 44 + 12 * children
                clone[at:at] = struct.pack("<hhII", 0, 0, 0x05000000, 0)
                clone[40] = children + 1
                new_uid = next_uid(9)
                location = append(clone)
                new_entries.setdefault((theme, 9), []).append((new_uid, 0, *location))
                stock_widgets.append((new_uid, old_uid))

        widget_uid = next_uid(9)
        image_uid = translation_uid = None
        if args.circle_app == "nfc":
            image_uid = next_uid(2)
            translation_uid = next_uid(6)
            with Image.open(args.assets / f"theme_{theme}.png") as image:
                image = image.convert("RGBA")
            require(image.size == (84, 84), "Shortcut icon must be 84x84")
            image_payload = rebuild_image(2, image.size,
                                          [encode_frame(image.tobytes(), *image.size)])
            image_location = append(image_payload)
            translation_location = append(translations("NFC-метки"))

        widget = bytearray(template)
        if args.circle_app == "nfc":
            struct.pack_into("<I", widget, 4, translation_uid)
            struct.pack_into("<I", widget, 36, image_uid)
            struct.pack_into("<hhII", widget, 44, 0, 0, image_uid, 0)
        struct.pack_into("<H", widget, 42,
                         (template_flags & ~0xff0) | (app_id << 4))
        if theme in NORMAL:
            widget = bytearray(widget[:56] + struct.pack("<hhII", 0, 0, 0x05000000, 0)
                               + widget[56:])
            widget[40] = 2
        struct.pack_into("<IHH", widget, len(widget) - 8, editbox_uid, 84, 84)
        widget_location = append(widget)

        new_slot = bytearray(slot)
        for index, (new_uid, _) in enumerate(stock_widgets):
            struct.pack_into("<I", new_slot, 4 + index * 4, new_uid)
        struct.pack_into("<I", new_slot, 4 + (slot_count - 1) * 4, widget_uid)
        new_slot_location = append(new_slot)
        struct.pack_into("<II", output, slot_index_pos + 8, *new_slot_location)
        allowed.update(range(slot_index_pos + 8, slot_index_pos + 16))

        if theme in NORMAL:
            descriptor = 168 + theme * 176 + 8 + 5 * 8
            struct.pack_into("<II", output, descriptor, len(file_records), file_table)
            allowed.update(range(descriptor, descriptor + 8))

        image_entries = [(editbox_uid, 0, *editbox_location)]
        if image_uid is not None:
            image_entries.insert(0, (image_uid, 0, *image_location))
        new_entries.setdefault((theme, 2), []).extend(image_entries)
        if translation_uid is not None:
            new_entries.setdefault((theme, 6), []).append(
                (translation_uid, 0, *translation_location))
        new_entries.setdefault((theme, 9), []).append((widget_uid, 0, *widget_location))
        changed_slots[theme, slot_uid] = new_slot_location
        report_themes.append({
            "theme": theme,
            "mode": node.get("type"),
            "slot": slot_name,
            "slot_uid": hex(slot_uid),
            "replaced_empty_uid": hex(last_uid),
            "widget_uid": hex(widget_uid),
            "image_uid": hex(image_uid) if image_uid is not None else None,
            "translation_uid": hex(translation_uid) if translation_uid is not None else None,
            "app_id": app_id,
            "lua_child": theme in NORMAL,
            "stock_widget_clones": len(stock_widgets),
            "stock_options_preserved": slot_count - 1,
        })

    for theme in range(8):
        for kind in (2, 6, 9):
            old_table = [(uid, 0, off, size) for t, k, uid, off, size, _ in original_entries
                         if t == theme and k == kind]
            new_table = new_entries.get((theme, kind), [])
            combined = sorted(old_table + new_table, key=lambda item: item[0])
            require(len({item[0] for item in combined}) == len(combined),
                    "Duplicate UID in rebuilt table")
            table_offset, _ = append(b"".join(struct.pack("<IIII", *item) for item in combined))
            descriptor = 168 + theme * 176 + 8 + kind * 8
            struct.pack_into("<II", output, descriptor, len(combined), table_offset)
            allowed.update(range(descriptor, descriptor + 8))

    output[0x28:0x68] = face_id.encode().ljust(64, b"\0")
    struct.pack_into("<II", output, 0x74, *append(translations(face_name)))

    after = {(t, u): (off, size, pos) for t, k, u, off, size, pos in entries(output)}
    unchanged = 0
    for t, k, uid, off, size, pos in original_entries:
        if (t, uid) in changed_slots:
            continue
        require(after[t, uid][:2] == (off, size), "Existing resource remapped")
        require(output[off:off + size] == original[off:off + size], "Existing resource bytes changed")
        unchanged += 1

    for theme in range(8):
        slot_uid = names[[n.get("ref")[1:] for n in xml.findall("Theme")[theme]
                          if n.get("x") == SLOT_X and n.get("y") == SLOT_Y][0]]
        off, size, _ = after[theme, slot_uid]
        slot = output[off:off + size]
        require(size == 188 and struct.unpack_from("<BBH", slot) == (44, 0, 2),
                "New Slot layout mismatch")
        require(slot[4 + 44 * 4:] == bytes.fromhex("0400000033000000"),
                "New Slot trailer changed")
        uids = struct.unpack_from("<" + "I" * 44, slot, 4)
        require(uids[-1] == int(report_themes[theme]["widget_uid"], 16),
                "New Slot does not reference the new Widget")
        if theme in NORMAL:
            for index in range(43):
                old_uid = original_slot_uids[theme][index]
                old_off, old_size = old[theme, old_uid][:2]
                old_widget = original[old_off:old_off + old_size]
                new_off, new_size = after[theme, uids[index]][:2]
                new_widget = output[new_off:new_off + new_size]
                require(new_size == old_size + 12 and
                        new_widget[40] == old_widget[40] + 1,
                        "Stock Widget clone layout changed")
                require(struct.unpack_from("<hhII", new_widget,
                                           44 + 12 * old_widget[40]) ==
                        (0, 0, 0x05000000, 0), "Stock Widget Lua child missing")
                old_flags = struct.unpack_from("<H", old_widget, 42)[0]
                new_flags = struct.unpack_from("<H", new_widget, 42)[0]
                require((old_flags >> 4) & 255 == (new_flags >> 4) & 255,
                        "Stock Widget action changed")
        else:
            require(list(uids[:43]) == original_slot_uids[theme][:43],
                    "Stock Slot options changed")

    for theme in range(8):
        widget_uid = int(report_themes[theme]["widget_uid"], 16)
        off, size, _ = after[theme, widget_uid]
        widget = output[off:off + size]
        require(widget[40] == (2 if theme in NORMAL else 1), "Widget child count mismatch")
        require(struct.unpack_from("<H", widget, 42)[0] & 2, "Widget action flag missing")
        runtime_flags = ((struct.unpack_from("<H", widget, 42)[0] & 0xf)
                         | (((struct.unpack_from("<H", widget, 42)[0] >> 4) & 255) << 7)
                         | (((struct.unpack_from("<H", widget, 42)[0] >> 12) & 7) << 4))
        require((runtime_flags >> 7) & 255 == app_id, "Native action did not roundtrip")
        if theme in NORMAL:
            require(struct.unpack_from("<hhII", widget, 56) == (0, 0, 0x05000000, 0),
                    "Lua child mismatch")
            for i, (name, payload) in enumerate(file_payloads):
                foff, fsize, _ = after[theme, 0x05000000 + i]
                require(decode_file(output[foff:foff + fsize]) == (name, payload),
                        "FileResource roundtrip mismatch")
        if report_themes[theme]["image_uid"] is not None:
            image_uid = int(report_themes[theme]["image_uid"], 16)
            ioff, isize, _ = after[theme, image_uid]
            isize2, frames = parse_image(output[ioff:ioff + isize], 2)
            require(isize2 == (84, 84), "New icon dimensions changed")

    changed = [i for i, (a, b) in enumerate(zip(original, output)) if a != b]
    require(set(changed) <= allowed, "Unexpected original-prefix mutation")
    require(sha(args.baseline.read_bytes()) == BASE_SHA, "Baseline changed during build")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    report = {
        "name": face_name,
        "id": face_id,
        "app_id": app_id,
        "app_name": "dev.mbapdu.tags",
        "firmware_sha256": FW_SHA,
        "baseline_sha256": BASE_SHA,
        "source_zip_sha256": SOURCE_SHA,
        "output_sha256": sha(output),
        "bytes": len(output),
        "changed_empty_variant": True,
        "stock_options_preserved_per_theme": 43,
        "themes": report_themes,
        "unchanged_original_resource_entries": unchanged,
        "changed_original_prefix_bytes": len(changed),
        "hardware_tested": False,
        "selection": ("Installer: lower circle is a Find Phone clone with the bootstrap overlay; "
                      "after setup it launches stock Find Phone" if args.circle_app == "findPhone"
                      else "Roundabout watchface editor: choose a stock app or NFC-метки in the lower Slot"),
        "limits": ([
            "The empty lower-circle option is replaced by a Find Phone installer clone; 43 stock choices remain.",
            "First tap loads modules, waits READY, writes /data/tag_catalog/auto_load and reboots.",
            "After reboot the overlay is transparent; the circle launches stock Find Phone and apps come from the menu.",
            "AOD themes get no Lua child and no installer.",
            "Device UI/reboot behavior is not verified.",
        ] if args.circle_app == "findPhone" else [
            "The empty lower-circle option is replaced by NFC-метки; 43 stock choices remain.",
            "Selecting NFC-метки keeps the existing manual bootstrap; stock choices launch natively.",
            "AOD themes get the icon/action but no Lua child.",
            "No runtime picker page is added; selection is the watchface editor Slot.",
            "Device UI/editor behavior is not verified.",
        ]),
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"output": str(args.output), "sha256": sha(output),
                      "bytes": len(output), "validated": True}, indent=2))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--baseline", type=Path,
                   default=ROOT / "releases/roundabout/roundabout-ru-p67glt.face")
    p.add_argument("--source-zip", type=Path,
                   default=ROOT / "references/roundabout-original.zip")
    p.add_argument("--firmware", type=Path, default=ROOT / "references/vela_ap.bin")
    p.add_argument("--recorder", type=Path, default=ROOT / "references/recorder.face")
    p.add_argument("--lua", type=Path, default=Path(__file__).with_name("main.lua"))
    p.add_argument("--core", type=Path, required=True)
    p.add_argument("--ui", type=Path, required=True)
    p.add_argument("--assets", type=Path,
                   default=ROOT / "tags/roundabout/shortcut-assets")
    p.add_argument("--circle-app", choices=("nfc", "findPhone"), default="nfc",
                   help="nfc: install NFC-метки entry; findPhone: stock installer button")
    p.add_argument("--face-id", default=FACE_ID)
    p.add_argument("--face-name", default=FACE_NAME)
    p.add_argument("--output", type=Path,
                   default=ROOT / "build/roundabout-ru-nfc5.face")
    p.add_argument("--report", type=Path,
                   default=Path(__file__).with_name("choice_validation.json"))
    build(p.parse_args())


if __name__ == "__main__":
    main()
