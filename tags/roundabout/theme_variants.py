#!/usr/bin/env python3
"""Build colour variants of the NFC 5 Roundabout face.

The watchface/editor format has exactly four normal styles and four AOD themes.
It cannot hold nine additional styles in one .face. Each variant is therefore a
separate .face with the same four styles, where one style is recoloured.

Base face: build/roundabout-ru-nfc5.face (ID 999999989), built by
tags/bootstrap/build_choice_face.py. The lower-circle app choice is preserved.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path
import math
import re
import struct
import zipfile
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont

from pack_roundabout import (decode_frame, encode_frame, index_entries,
                             parse_image, rebuild_image)

ROOT = Path(__file__).resolve().parents[2]
BASE_FACE = ROOT / "build/roundabout-ru-nfc5.face"
BASE_SHA = "d650dd4c3cd9ce7f5a5a2da5ccff4e410e6ffbaf97aeafbda178769bf3a2c560"
SOURCE_ZIP = ROOT / "references/roundabout-original.zip"
OUT_DIR = ROOT / "build/theme-variants"
REPORT = ROOT / "tags/roundabout/themes_validation.json"
PREVIEW_SHEET = ROOT / "tags/roundabout/themes-preview.png"
NORMAL_FACE = (0, 2, 3, 4)
AOD_FACE = (1, 5, 6, 7)
AOD_BASE = {4: 1, 7: 7}

VARIANTS = [
    dict(name="NFC 5 Light Sapphire", face_id="999999988", base="light", slug="light-sapphire",
         bezel=(64, 120, 200), hand=(41, 121, 255)),
    dict(name="NFC 5 Light Emerald", face_id="999999987", base="light", slug="light-emerald",
         bezel=(46, 158, 107), hand=(0, 200, 83)),
    dict(name="NFC 5 Light Turquoise", face_id="999999986", base="light", slug="light-turquoise",
         bezel=(0, 168, 150), hand=(0, 200, 180)),
    dict(name="NFC 5 Dark Turquoise", face_id="999999985", base="dark", slug="dark-turquoise",
         bezel=(0, 191, 166), hand=(0, 229, 199)),
    dict(name="NFC 5 Copper", face_id="999999984", base="dark", slug="copper",
         bezel=(184, 115, 51), hand=(255, 183, 77)),
    dict(name="NFC 5 Deep Ocean", face_id="999999983", base="dark", slug="deep-ocean",
         bezel=(30, 95, 138), hand=(79, 195, 247)),
    dict(name="NFC 5 Sunset Coral", face_id="999999982", base="dark", slug="sunset-coral",
         bezel=(224, 90, 79), hand=(255, 138, 101)),
    dict(name="NFC 5 Forest Sage", face_id="999999981", base="light", slug="forest-sage",
         bezel=(78, 125, 91), hand=(129, 199, 132)),
    dict(name="NFC 5 Steel", face_id="999999980", base="dark", slug="steel",
         bezel=(154, 164, 176), hand=(224, 224, 224)),
]
BASE_STYLE = {"dark": 0, "light": 3}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def translations(text):
    text = text.encode("utf8")
    return struct.pack("<Q33I", (1 << 33) - 1, *([len(text)] * 33)) + text * 33


def clamp(value):
    return max(0, min(255, int(round(value))))


def scale_colour(colour, factor):
    return tuple(clamp(c * factor) for c in colour)


def quantize(image):
    image = image.convert("RGBA")
    image = image.quantize(colors=256, method=Image.Quantize.FASTOCTREE,
                           dither=Image.Dither.NONE).convert("RGBA")
    require(len(set(image.getdata())) <= 256, "Quantized image exceeds 256 colours")
    return image


def recolor_background(image, target, base, low, high):
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
            f = brightness / base
            pixels[x, y] = (clamp(target[0] * f), clamp(target[1] * f),
                            clamp(target[2] * f), a)
    return out


def recolor_hand(image, target, base):
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
            f = brightness / base
            pixels[x, y] = (clamp(target[0] * f), clamp(target[1] * f),
                            clamp(target[2] * f), a)
    return out


def dominant_hand_colour(image):
    counts = Counter()
    for r, g, b, a in image.getdata():
        if a > 180 and max(r, g, b) - min(r, g, b) > 20 and (r + g + b) / 3 > 80:
            counts[(r, g, b)] += 1
    require(counts, "Minute hand has no coloured pixels")
    return counts.most_common(1)[0][0]


def recolor_preview(image, target, base, base_hand):
    out = recolor_background(image, target, base, 120, 242)
    pixels = out.load()
    width, height = out.size
    for y in range(40, min(440, height)):
        for x in range(40, min(320, width)):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            if abs(r - base_hand[0]) + abs(g - base_hand[1]) + abs(b - base_hand[2]) < 100:
                brightness = (r + g + b) / 3
                f = max(0.5, min(1.1, brightness / 200))
                pixels[x, y] = (clamp(target[0] * f), clamp(target[1] * f),
                                clamp(target[2] * f), a)
    return out


def image_uids(editor, names, src_to_name):
    result = {}
    for editor_index, theme in enumerate(editor["themes"]):
        for child in theme.get("children", []):
            label = child.get("attrs", {}).get("name")
            if label not in ("背景", "背景 (1)", "分针"):
                continue
            resources = child.get("attrs", {}).get("resources", {})
            pointer = resources.get("pointer0", {})
            if not pointer:
                continue
            filename = next(iter(pointer.values()))[0]
            image_name = src_to_name.get(filename)
            require(image_name is not None, "No manifest image for " + filename)
            key = "bg" if label.startswith("背景") else "minute"
            result[(editor_index, key)] = names[image_name]
    return result


def decode_image(face, entry):
    theme, kind, uid, offset, size, position = entry
    size_pair, frames = parse_image(face[offset:offset + size], kind)
    return Image.frombytes("RGBA", size_pair, decode_frame(frames[0], *size_pair)), size_pair, position


def build_variant(base, variant, editor, names, src_to_name):
    face = bytearray(base)
    base_entries = list(index_entries(base))
    old = {(t, u): (off, size, pos) for t, k, u, off, size, pos in base_entries}
    allowed = set(range(0x28, 0x68)) | set(range(0x74, 0x7c))
    modified = set()
    uids = image_uids(editor, names, src_to_name)
    style = BASE_STYLE[variant["base"]]
    normal_editor = style
    normal_face = NORMAL_FACE[style]
    aod_editor = 4 + style
    aod_face = AOD_BASE[aod_editor]
    dark = variant["base"] == "dark"
    bezel = variant["bezel"]
    hand = variant["hand"]
    aod_bezel = scale_colour(bezel, 0.45)
    aod_hand = scale_colour(hand, 0.65)

    def append(payload):
        while len(face) % 4:
            face.append(0)
        at = len(face)
        face.extend(payload)
        return at, len(payload)

    def replace(theme, uid, recolour):
        entry = next((e for e in base_entries if e[0] == theme and e[2] == uid), None)
        require(entry is not None, "Missing image resource " + hex(uid))
        image, size_pair, position = decode_image(base, entry)
        require(size_pair in ((336, 480), (52, 480)), "Unexpected source image size")
        changed = quantize(recolour(image))
        payload = rebuild_image(2, size_pair,
                                [encode_frame(changed.tobytes(), *size_pair)])
        location = append(payload)
        struct.pack_into("<II", face, position + 8, *location)
        allowed.update(range(position + 8, position + 16))
        modified.add((theme, uid))
        return image

    # Normal style: background ring and minute hand.
    original_minute = replace(normal_face, uids[(normal_editor, "minute")],
                              lambda image: recolor_hand(image, hand, 200))
    base_hand = dominant_hand_colour(original_minute)
    replace(normal_face, uids[(normal_editor, "bg")],
            lambda image: recolor_background(image, bezel, 204 if dark else 213,
                                             100 if dark else 120, 242))

    # AOD: dim ring and dim minute hand.
    replace(aod_face, uids[(aod_editor, "minute")],
            lambda image: recolor_hand(image, aod_hand, 190))
    replace(aod_face, uids[(aod_editor, "bg")],
            lambda image: recolor_background(image, aod_bezel, 54, 15, 120))

    # Gallery style preview for the normal style.
    preview_pointer = 168 + normal_face * 176 + 4
    preview_offset = u32(base, preview_pointer)
    preview_length = 12 + u32(base, preview_offset + 8)
    preview_image, preview_size, _ = decode_image(
        base, (normal_face, 2, 0, preview_offset, preview_length, 0))
    require(preview_size == (336, 480), "Unexpected preview size")
    preview_changed = quantize(recolor_preview(preview_image, bezel,
                                               204 if dark else 213, base_hand))
    preview_location = append(rebuild_image(
        2, preview_size, [encode_frame(preview_changed.tobytes(), *preview_size)]))
    struct.pack_into("<I", face, preview_pointer, preview_location[0])
    allowed.update(range(preview_pointer, preview_pointer + 4))
    # The face's gallery thumbnail should identify the variant. The default
    # style remains style 1; this pointer is only the gallery preview.
    struct.pack_into("<I", face, 0x20, preview_location[0])
    allowed.update(range(0x20, 0x24))

    face[0x28:0x68] = variant["face_id"].encode().ljust(64, b"\0")
    struct.pack_into("<II", face, 0x74, *append(translations(variant["name"])))

    after = {(t, u): (off, size, pos) for t, k, u, off, size, pos in index_entries(face)}
    for t, k, uid, offset, size, position in base_entries:
        if (t, uid) in modified:
            require(after[t, uid][0] != offset or after[t, uid][1] != size,
                    "Modified image kept the old location")
            continue
        require(after[t, uid][:2] == (offset, size), "Existing resource remapped")
        require(face[offset:offset + size] == base[offset:offset + size],
                "Existing resource bytes changed")
    for theme, uid in modified:
        off, size, _ = after[theme, uid]
        size_pair, _ = parse_image(face[off:off + size], 2)
        require(size_pair in ((336, 480), (52, 480)), "New image size mismatch")
    changed = [i for i, (a, b) in enumerate(zip(base, face)) if a != b]
    require(set(changed) <= allowed, "Unexpected original-prefix mutation")
    require(sha(base) == BASE_SHA, "Base face changed during build")
    return bytes(face), preview_changed, dict(
        name=variant["name"], face_id=variant["face_id"], base=variant["base"],
        slug=variant["slug"], bezel=list(bezel), hand=list(hand),
        normal_theme=normal_face, aod_theme=aod_face,
        background_uid=hex(uids[(normal_editor, "bg")]),
        minute_uid=hex(uids[(normal_editor, "minute")]),
        aod_background_uid=hex(uids[(aod_editor, "bg")]),
        aod_minute_uid=hex(uids[(aod_editor, "minute")]),
        sha256=sha(face), bytes=len(face), hardware_tested=False)


def contact_sheet(previews, labels, output):
    width, height = 336, 480
    gap, label_h = 18, 34
    cols, rows = 3, 3
    sheet = Image.new("RGB", (cols * width + (cols + 1) * gap,
                              rows * (height + label_h) + (rows + 1) * gap),
                      (7, 10, 14))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 18)
    for index, (image, label) in enumerate(zip(previews, labels)):
        col, row = index % cols, index // cols
        x = gap + col * (width + gap)
        y = gap + row * (height + label_h + gap)
        draw.text((x, y), label, font=font, fill=(230, 238, 246))
        sheet.paste(image.convert("RGB"), (x, y + label_h))
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-face", type=Path, default=BASE_FACE)
    parser.add_argument("--source-zip", type=Path, default=SOURCE_ZIP)
    parser.add_argument("--out-dir", type=Path, default=OUT_DIR)
    parser.add_argument("--report", type=Path, default=REPORT)
    parser.add_argument("--preview-sheet", type=Path, default=PREVIEW_SHEET)
    args = parser.parse_args()

    base = args.base_face.read_bytes()
    require(sha(base) == BASE_SHA, "Unexpected NFC 5 base face")
    with zipfile.ZipFile(args.source_zip) as z:
        editor = json.loads(z.read("editor.config.json"))
        names = {k: int(v, 16) for k, v in
                 re.findall(r"^(\w+):\s*([0-9a-fA-F]+)",
                            z.read("uidmap.map").decode(), re.M)}
        manifest = z.read("resources/manifest.xml").decode("utf8", "replace")
    src_to_name = {m.group(2): m.group(1) for m in
                   re.finditer(r'<Image name="([^"]+)" src="([^"]+)"', manifest)}
    require(len(editor["themes"]) == 8, "Unexpected editor theme count")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    records = []
    previews = []
    labels = []
    for variant in VARIANTS:
        face, preview, record = build_variant(base, variant, editor, names, src_to_name)
        face_path = args.out_dir / (variant["slug"] + ".face")
        face_path.write_bytes(face)
        preview_path = args.out_dir / (variant["slug"] + ".png")
        preview.convert("RGB").save(preview_path)
        record["face_path"] = str(face_path)
        record["preview_path"] = str(preview_path)
        records.append(record)
        previews.append(preview)
        labels.append(variant["name"] + "  ·  " + variant["face_id"])

    sheet = contact_sheet(previews, labels, args.preview_sheet)

    def relative(path):
        resolved = Path(path).resolve()
        try:
            return str(resolved.relative_to(ROOT.resolve()))
        except ValueError:
            return str(resolved)

    report_variants = []
    for record in records:
        item = dict(record)
        item["face_path"] = relative(record["face_path"])
        item["preview_path"] = relative(record["preview_path"])
        item["preview_sheet"] = relative(sheet)
        report_variants.append(item)
    report = {
        "schema": 1,
        "base_face": relative(args.base_face),
        "base_sha256": BASE_SHA,
        "style_limit": "4 normal styles + 4 AOD themes per .face; variants are separate faces",
        "variants": report_variants,
        "contact_sheet": relative(sheet),
        "hardware_tested": False,
        "limits": [
            "Each variant replaces one base style (light style 4 or dark style 1) in a separate .face.",
            "The other three styles remain; the variant is selected in the Roundabout editor.",
            "The lower-circle app choice from NFC 5 is preserved.",
            "AOD ring/minute hand are recoloured dimmer; AOD previews are not separate.",
            "Previews are renders, not device screenshots.",
        ],
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")

    zip_path = args.out_dir / "roundabout-ru-nfc5-themes.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
        for record in records:
            archive.write(record["face_path"], Path(record["face_path"]).name)
            archive.write(record["preview_path"], Path(record["preview_path"]).name)
        archive.write(sheet, sheet.name)
        docs = ROOT / "tags/roundabout/THEMES.md"
        if docs.exists():
            archive.write(docs, docs.name)
        archive.writestr("manifest.json", json.dumps(report, ensure_ascii=False, indent=2))
    print(json.dumps({"count": len(records), "zip": str(zip_path),
                      "sheet": str(sheet), "report": str(args.report)}, indent=2))


if __name__ == "__main__":
    main()
