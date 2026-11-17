#!/usr/bin/env python3
"""Build one NFC 5 face with four selected themes (two light, two dark).

Style order: 1 Dark Turquoise, 2 Light Sapphire, 3 Deep Ocean, 4 Light Turquoise.
The face format has one light base style; style 2 is a full clone of style 4
(resources/tables), then recoloured. Light themes use pre-blended "semi-
transparent" accent on the bezel, sun progress track and both bottom bars.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import math
import re
import struct
import zipfile
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont

from pack_roundabout import (decode_frame, encode_frame, index_entries,
                             parse_image, rebuild_image)
from theme_variants import (dominant_hand_colour, image_uids, quantize,
                            translations)

ROOT = Path(__file__).resolve().parents[2]
BASE_FACE = ROOT / "build/roundabout-ru-nfc5.face"
BASE_SHA = "d650dd4c3cd9ce7f5a5a2da5ccff4e410e6ffbaf97aeafbda178769bf3a2c560"
SOURCE_ZIP = ROOT / "references/roundabout-original.zip"
FACE_ID = "999999979"
FACE_NAME = "NFC 5 Four"
OUT = ROOT / "build/roundabout-ru-nfc5-four.face"
REPORT = ROOT / "tags/roundabout/four_validation.json"
PREVIEW = ROOT / "tags/roundabout/four-preview.png"
STYLES = [
    dict(name="Dark Turquoise", face=0, aod=1, editor=0, aod_editor=4,
         accent=(0, 191, 166), hand=(0, 229, 199), light=False),
    dict(name="Light Sapphire", face=2, aod=5, editor=3, aod_editor=7,
         accent=(64, 120, 200), hand=(41, 121, 255), light=True, clone=True),
    dict(name="Deep Ocean", face=3, aod=6, editor=2, aod_editor=6,
         accent=(30, 95, 138), hand=(79, 195, 247), light=False),
    dict(name="Light Turquoise", face=4, aod=7, editor=3, aod_editor=7,
         accent=(0, 168, 150), hand=(0, 200, 180), light=True),
]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def clamp(value):
    return max(0, min(255, int(round(value))))


def blend(colour, page, factor):
    return tuple(clamp(colour[i] * factor + page[i] * (1 - factor)) for i in range(3))


def scale(colour, factor):
    return tuple(clamp(c * factor) for c in colour)


def source_name_map(z):
    root = ET.fromstring(z.read("resources/manifest.xml"))
    mapping = {}
    for element in root.find("Resources"):
        if element.tag == "Image" and element.get("name") and element.get("src"):
            mapping[element.get("src")] = element.get("name")
        elif element.tag == "ImageArray" and element.get("name"):
            for child in element:
                mapping[child.get("src")] = element.get("name")
    return mapping


def first_image_src(resources):
    for key, value in resources.items():
        if key == "images":
            for _, sources in value.items():
                return sources[0]
    return None


def special_uids(editor, ti, names, src_to_name):
    theme = editor["themes"][ti]
    child = next(c for c in theme["children"]
                 if c.get("attrs", {}).get("name") == "时间段")
    sun_name = src_to_name[first_image_src(child["attrs"]["resources"])]
    sun = names[sun_name]
    bars = {}
    for slot in theme["children"]:
        if slot.get("type") != "Slot" or slot.get("attrs", {}).get("y") != 364:
            continue
        side = "left" if slot.get("attrs", {}).get("x", 0) < 0 else "right"
        for widget in slot.get("children", []):
            if widget.get("attrs", {}).get("relatedDataKey") not in ("step", "kcal"):
                continue
            for element in widget.get("children", []):
                if element.get("type") == "ImageArray" and \
                        element.get("attrs", {}).get("name") == "图组1":
                    bar_name = src_to_name[first_image_src(element["attrs"]["resources"])]
                    bars[side] = names[bar_name]
                    break
            if side in bars:
                break
    require(set(bars) == {"left", "right"}, "Missing bottom bars for theme")
    return sun, bars


def recolor_background(image, target, base, low, high, page, factor):
    out = image.copy()
    pixels = out.load()
    width, height = out.size
    cx, cy = width / 2, height / 2
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            if math.hypot(x - cx, y - cy) < 128:
                continue
            if max(r, g, b) - min(r, g, b) > 24:
                continue
            brightness = (r + g + b) / 3
            if not (low <= brightness <= high):
                continue
            raw = scale(target, brightness / base)
            pixels[x, y] = (*blend(raw, page, factor), a)
    return out


def recolor_hand(image, target):
    out = image.copy()
    pixels = out.load()
    width, height = out.size
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            brightness = (r + g + b) / 3
            if brightness < 40:
                continue
            pixels[x, y] = (*scale(target, brightness / 200), a)
    return out


def recolor_sun(image, target, page, track_factor):
    out = image.copy()
    pixels = out.load()
    width, height = out.size
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            brightness = (r + g + b) / 3
            saturation = max(r, g, b) - min(r, g, b)
            if saturation > 30 or brightness > 240:
                pixels[x, y] = (*target, a)
            else:
                raw = scale(target, brightness / 213 if page == (255, 255, 255) else brightness / 172)
                pixels[x, y] = (*blend(raw, page, track_factor), a)
    return out


def recolor_bars(image, target, page, factor):
    out = image.copy()
    pixels = out.load()
    width, height = out.size
    base = 213 if page == (255, 255, 255) else 122
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            brightness = (r + g + b) / 3
            raw = scale(target, brightness / base)
            pixels[x, y] = (*blend(raw, page, factor), a)
    return out


def recolor_preview(image, target, page, factor, base_hand):
    out = recolor_background(image, target, 213 if page == (255, 255, 255) else 204,
                             120 if page == (255, 255, 255) else 100, 242,
                             page, factor)
    pixels = out.load()
    width, height = out.size
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            if 140 <= x <= 196 and 10 <= y <= 60:
                saturation = max(r, g, b) - min(r, g, b)
                if saturation > 30 or (page == (0, 0, 0) and (r + g + b) / 3 > 240):
                    pixels[x, y] = (*target, a)
                    continue
            if abs(r - base_hand[0]) + abs(g - base_hand[1]) + abs(b - base_hand[2]) < 100:
                brightness = (r + g + b) / 3
                pixels[x, y] = (*scale(target, max(0.5, min(1.1, brightness / 200))), a)
    return out


def decode_entry(face, entry):
    theme, kind, uid, offset, size, position = entry
    size_pair, frames = parse_image(face[offset:offset + size], kind)
    return size_pair, frames


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-face", type=Path, default=BASE_FACE)
    parser.add_argument("--source-zip", type=Path, default=SOURCE_ZIP)
    parser.add_argument("--output", type=Path, default=OUT)
    parser.add_argument("--report", type=Path, default=REPORT)
    parser.add_argument("--preview", type=Path, default=PREVIEW)
    args = parser.parse_args()

    base = args.base_face.read_bytes()
    require(sha(base) == BASE_SHA, "Unexpected NFC 5 base face")
    with zipfile.ZipFile(args.source_zip) as z:
        editor = json.loads(z.read("editor.config.json"))
        names = {k: int(v, 16) for k, v in
                 re.findall(r"^(\w+):\s*([0-9a-fA-F]+)",
                            z.read("uidmap.map").decode(), re.M)}
        src_to_name = source_name_map(z)
    require(len(editor["themes"]) == 8, "Unexpected editor themes")
    bg_min = image_uids(editor, names, src_to_name)

    face = bytearray(base)
    base_entries = list(index_entries(base))
    old_positions = {(t, k, u): pos for t, k, u, off, size, pos in base_entries}
    allowed = set(range(0x28, 0x68)) | set(range(0x74, 0x7c))

    def append(payload):
        while len(face) % 4:
            face.append(0)
        at = len(face)
        face.extend(payload)
        return at, len(payload)

    def clone_theme(src, dst):
        src_desc = 168 + src * 176
        dst_desc = 168 + dst * 176
        face[dst_desc:dst_desc + 176] = base[src_desc:src_desc + 176]
        allowed.update(range(dst_desc, dst_desc + 176))
        for kind in range(12):
            count, table = struct.unpack_from("<II", base, src_desc + 8 + 8 * kind)
            new_table, _ = append(base[table:table + count * 16])
            struct.pack_into("<II", face, dst_desc + 8 + 8 * kind, count, new_table)

    # Clone the light style 4 into style 2 for the second light theme.
    clone_theme(4, 2)
    clone_theme(7, 5)

    positions = dict(old_positions)
    for theme in (2, 5):
        desc = 168 + theme * 176
        for kind in range(12):
            count, table = struct.unpack_from("<II", face, desc + 8 + 8 * kind)
            for i in range(count):
                pos = table + i * 16
                uid = struct.unpack_from("<I", face, pos)[0]
                positions[(theme, kind, uid)] = pos

    modified = set()

    def rebuild_positions(theme):
        desc = 168 + theme * 176
        for kind in range(12):
            count, table = struct.unpack_from("<II", face, desc + 8 + 8 * kind)
            for i in range(count):
                pos = table + i * 16
                uid = struct.unpack_from("<I", face, pos)[0]
                positions[(theme, kind, uid)] = pos

    def ensure_entry(theme, kind, uid):
        if (theme, kind, uid) in positions:
            return
        source = next(e for e in base_entries if e[1] == kind and e[2] == uid)
        desc = 168 + theme * 176
        count, table = struct.unpack_from("<II", face, desc + 8 + 8 * kind)
        entries = face[table:table + count * 16] + struct.pack(
            "<IIII", uid, 0, source[3], source[4])
        new_table, _ = append(entries)
        struct.pack_into("<II", face, desc + 8 + 8 * kind, count + 1, new_table)
        allowed.update(range(desc + 8 + 8 * kind, desc + 16 + 8 * kind))
        rebuild_positions(theme)

    def replace(theme, uid, kind, recolour):
        key = (theme, kind, uid)
        require(key in positions, "Missing target resource " + hex(uid))
        pos = positions[key]
        offset, size = struct.unpack_from("<II", face, pos + 8)
        size_pair, frames = decode_entry(face, (theme, kind, uid, offset, size, pos))
        if kind == 2:
            require(len(frames) == 1, "Single image expected")
            image = Image.frombytes("RGBA", size_pair, decode_frame(frames[0], *size_pair))
            changed = quantize(recolour(image))
            payload = rebuild_image(2, size_pair,
                                    [encode_frame(changed.tobytes(), *size_pair)])
        else:
            new_frames = []
            for frame in frames:
                image = Image.frombytes("RGBA", size_pair, decode_frame(frame, *size_pair))
                changed = quantize(recolour(image))
                new_frames.append(encode_frame(changed.tobytes(), *size_pair))
            payload = rebuild_image(3, size_pair, new_frames)
        location = append(payload)
        struct.pack_into("<II", face, pos + 8, *location)
        allowed.update(range(pos + 8, pos + 16))
        modified.add((theme, kind, uid))

    previews = []
    labels = []
    records = []
    for style in STYLES:
        theme = style["face"]
        aod = style["aod"]
        editor_theme = style["editor"]
        aod_editor = style["aod_editor"]
        light = style["light"]
        page = (255, 255, 255) if light else (0, 0, 0)
        factor = 0.55 if light else 0.85
        bar_factor = 0.55 if light else 0.80
        accent = style["accent"]
        hand = style["hand"]

        # Normal background and minute hand.
        replace(theme, bg_min[(editor_theme, "minute")], 2, lambda im: recolor_hand(im, hand))
        replace(theme, bg_min[(editor_theme, "bg")], 2,
                lambda im: recolor_background(im, accent,
                                              213 if light else 204,
                                              120 if light else 100, 242,
                                              page, factor))
        sun_uid, bars = special_uids(editor, editor_theme, names, src_to_name)
        ensure_entry(theme, 3, sun_uid)
        ensure_entry(theme, 3, bars["left"])
        ensure_entry(theme, 3, bars["right"])
        replace(theme, sun_uid, 3,
                lambda im: recolor_sun(im, accent, page, bar_factor))
        replace(theme, bars["left"], 3,
                lambda im: recolor_bars(im, accent, page, bar_factor))
        replace(theme, bars["right"], 3,
                lambda im: recolor_bars(im, accent, page, bar_factor))

        # AOD background and minute hand: dim accent.
        aod_bg = bg_min[(aod_editor, "bg")]
        aod_min = bg_min[(aod_editor, "minute")]
        dim = scale(accent, 0.45)
        dim_hand = scale(hand, 0.65)
        replace(aod, aod_bg, 2,
                lambda im: recolor_background(im, dim, 54, 15, 120, (0, 0, 0), 1.0))
        replace(aod, aod_min, 2, lambda im: recolor_hand(im, dim_hand))

        # Style preview. Style 2 uses the light preview source.
        source_theme = 4 if style.get("clone") else theme
        preview_pointer = 168 + theme * 176 + 4
        preview_offset = u32(base, 168 + source_theme * 176 + 4)
        preview_length = 12 + u32(base, preview_offset + 8)
        size_pair, frames = decode_entry(
            base, (source_theme, 2, 0, preview_offset, preview_length, 0))
        require(size_pair == (336, 480), "Unexpected preview size")
        preview_image = Image.frombytes("RGBA", size_pair, decode_frame(frames[0], *size_pair))
        # Sample the minute-hand colour from the normal minute hand resource.
        minute_entry = next(e for e in base_entries
                            if e[0] == source_theme and e[2] == bg_min[(editor_theme, "minute")])
        minute_size, minute_frames = decode_entry(base, minute_entry)
        minute_image = Image.frombytes("RGBA", minute_size,
                                       decode_frame(minute_frames[0], *minute_size))
        base_hand = dominant_hand_colour(minute_image)
        changed_preview = quantize(recolor_preview(preview_image, accent, page, factor, base_hand))
        preview_location = append(rebuild_image(
            2, size_pair, [encode_frame(changed_preview.tobytes(), *size_pair)]))
        struct.pack_into("<I", face, preview_pointer, preview_location[0])
        allowed.update(range(preview_pointer, preview_pointer + 4))
        previews.append(changed_preview)
        labels.append(style["name"])
        records.append({
            "name": style["name"], "style": len(records) + 1,
            "normal_descriptor": theme, "aod_descriptor": aod,
            "window": "light" if light else "dark",
            "bezel": list(accent), "hand": list(hand),
            "semi_transparent_blend": factor,
        })

    # Main gallery thumbnail: style 1 preview.
    main_offset = u32(face, 168 + 4)
    struct.pack_into("<I", face, 0x20, main_offset)
    allowed.update(range(0x20, 0x24))
    face[0x28:0x68] = FACE_ID.encode().ljust(64, b"\0")
    struct.pack_into("<II", face, 0x74, *append(translations(FACE_NAME)))

    # Validation.
    after = {(t, k, u): (off, size, pos) for t, k, u, off, size, pos in index_entries(face)}
    for t, k, uid, offset, size, pos in base_entries:
        if t in (2, 5):
            continue
        if (t, k, uid) in modified:
            continue
        require(after[t, k, uid][:2] == (offset, size), "Existing resource remapped")
        require(face[offset:offset + size] == base[offset:offset + size],
                "Existing resource bytes changed")
    for theme, source in ((2, 4), (5, 7)):
        left = {(k, u) for t, k, u, *_ in index_entries(face) if t == theme}
        right = {(k, u) for t, k, u, *_ in index_entries(base) if t == source}
        require(left == right, "Cloned theme resource set mismatch")
    for theme, kind, uid in modified:
        off, size, _ = after[theme, kind, uid]
        size_pair, _ = parse_image(face[off:off + size], kind)
        require(size_pair in ((336, 480), (52, 480), (166, 116), (336, 119)),
                "New image size mismatch")
    changed = [i for i, (a, b) in enumerate(zip(base, face)) if a != b]
    require(set(changed) <= allowed, "Unexpected original-prefix mutation")
    require(sha(base) == BASE_SHA, "Base face changed during build")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(face)
    # Contact sheet 2x2.
    sheet = Image.new("RGB", (2 * 336 + 3 * 18, 2 * (480 + 34) + 3 * 18), (7, 10, 14))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 18)
    for index, (image, label) in enumerate(zip(previews, labels)):
        col, row = index % 2, index // 2
        x = 18 + col * (336 + 18)
        y = 18 + row * (480 + 34 + 18)
        draw.text((x, y), label + "  ·  " + str(index + 1), font=font, fill=(230, 238, 246))
        sheet.paste(image.convert("RGB"), (x, y + 34))
    args.preview.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.preview)
    try:
        base_face_path = str(args.base_face.resolve().relative_to(ROOT.resolve()))
    except ValueError:
        base_face_path = str(args.base_face)
    report = {
        "name": FACE_NAME, "id": FACE_ID, "base_face": base_face_path,
        "base_sha256": BASE_SHA, "output_sha256": sha(face), "bytes": len(face),
        "styles": records,
        "light_blend": 0.55, "dark_blend": 0.85,
        "notes": [
            "Style 2 is a full clone of the light style 4; its light resources are recoloured separately.",
            "Light themes pre-blend accent with white to emulate a semi-transparent accent (indexed8, no reliance on screen compositing).",
            "Dark themes pre-blend accent with black; bezel, sun track and both bottom bars use the accent.",
            "Sun progress ImageArray and both bottom bar ImageArrays are recoloured.",
            "AOD themes are recoloured dimmer; no separate AOD previews.",
            "Hardware acceptance pending.",
        ],
        "hardware_tested": False,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"output": str(args.output), "sha256": sha(face),
                      "bytes": len(face), "preview": str(args.preview)}, indent=2))


if __name__ == "__main__":
    main()
