#!/usr/bin/env python3
"""Read-only evidence extraction for Roundabout and p67glt 3.201.026.

Does not contact a device, patch a watchface, or call firmware code.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET


FW_SHA256 = "66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40"
BASE = 0x2C0C0000
ACTION_OFFSET = 0x9B7D18
ACTION_COUNT = 39


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cstring(blob, offset):
    if not 0 <= offset < len(blob):
        raise ValueError("String pointer outside firmware")
    end = blob.index(b"\0", offset)
    return blob[offset:end].decode("ascii")


def audit(source, firmware=None):
    description = ET.parse(source / "description.xml").getroot()
    identity = {k: description.findtext(k) for k in ("name", "version", "pkgName", "deviceType", "size")}
    if identity != dict(name="Roundabout", version="1.1.20", pkgName="120917397222", deviceType="P67", size="336x480"):
        raise ValueError("Unexpected watchface identity; inspect it before reusing this analysis")
    manifest = ET.parse(source / "resources/manifest.xml").getroot()
    resources = manifest.find("Resources")
    by_name = {element.get("name"): element for element in resources}
    editor = json.loads((source / "editor.config.json").read_text())
    round_slots = []
    for theme_index, theme in enumerate(manifest.findall("Theme")):
        for layout in theme.findall("Layout"):
            element = by_name.get(layout.get("ref", "").removeprefix("@"))
            if element is None or element.tag != "Slot" or element.get("SlotGroupName") not in {"1", "2", "3"}:
                continue
            items = [by_name[item.get("ref").removeprefix("@")] for item in element.findall("Item")]
            round_slots.append(dict(
                theme_index=theme_index,
                theme_type=theme.get("type"),
                group=element.get("SlotGroupName"),
                resource=element.get("name"),
                x=int(layout.get("x")), y=int(layout.get("y")),
                item_count=len(items),
                action_keys=sorted({item.get("jumpApp") for item in items if item.get("jumpApp")}),
            ))
    if len(round_slots) != 24 or any(s["item_count"] != 44 for s in round_slots):
        raise ValueError("Roundabout slot layout differs from the analysed version")
    editor_slots = []
    for theme_index, theme in enumerate(editor["themes"]):
        for child in theme["children"]:
            attrs = child.get("attrs", {})
            group = {(55, 195): "1", (203, 195): "2", (126, 274): "3"}.get((attrs.get("x"), attrs.get("y")))
            if child["type"] == "Slot" and group:
                editor_slots.append(dict(theme_index=theme_index, id=child["id"], group=group,
                                         choices=len(child.get("children", [])), include_empty=attrs.get("includeEmpty"),
                                         selected_id=attrs.get("widgetIdToDisplay")))
    if len(editor_slots) != 24 or any(s["choices"] != 43 or not s["include_empty"] for s in editor_slots):
        raise ValueError("Editor choices differ from the analysed version")
    images = {element.get("src") for element in resources.iter("Image") if element.get("src")}
    absent = sorted(src for src in images if not (source / "resources" / src).is_file())
    report = dict(
        identity=identity,
        source_sha256={name: digest(source / name) for name in
                       ("description.xml", "editor.config.json", "resources/manifest.xml", "preview/preview.png")},
        round_slots=round_slots,
        editor_slots=editor_slots,
        unique_image_resources=len(images),
        missing_image_resources=len(absent),
        source_ready_for_repack=not absent,
        hardware_changed=False,
    )
    if firmware:
        actual = digest(firmware)
        if actual != FW_SHA256:
            raise ValueError("Firmware digest mismatch; fixed offsets are for p67glt 3.201.026 only")
        blob = firmware.read_bytes()
        actions = []
        for index in range(ACTION_COUNT):
            pointer, app_id = struct.unpack_from("<II", blob, ACTION_OFFSET + index * 8)
            actions.append(dict(key=cstring(blob, pointer - BASE), app_id=f"0x{app_id:02x}"))
        if actions[0] != dict(key="pressure", app_id="0x03") or actions[-1] != dict(key="navigation", app_id="0x48"):
            raise ValueError("Unexpected action table")
        assert struct.unpack_from("<I", blob, 0x708CFC)[0] == BASE + ACTION_OFFSET
        assert cstring(blob, 0x8CCA1C) == "wechat"
        assert cstring(blob, 0x8CCA24) == "[%s] %s: app %s not found\n"
        report["firmware"] = dict(
            sha256=actual,
            jump_handler_va="0x2c7c8c60",
            action_table_va=f"0x{BASE + ACTION_OFFSET:08x}",
            action_table_file_offset=f"0x{ACTION_OFFSET:x}",
            action_count=ACTION_COUNT,
            actions=actions,
            single_special_fallback="wechat -> com.tencent.wechatrtos",
            unknown_key_behavior="log app %s not found, return (verified by static disassembly)",
        )
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--firmware", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = audit(args.source, args.firmware)
    rendered = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.write_text(rendered)
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()
