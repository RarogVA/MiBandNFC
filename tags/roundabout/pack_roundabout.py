#!/usr/bin/env python3
"""Append-only Russian resource patch for the exact, user-provided Roundabout.

This is deliberately not a general Xiaomi compiler. It accepts one fingerprinted
original, the companion source tree and the audited localization map. Layouts,
actions and all original payloads remain in place. Only the ID, title pointer,
preview pointers and modified image index pointers change in the original prefix.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path
import xml.etree.ElementTree as ET

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
ORIGINAL_SHA256 = "ed1948a5ad2bd260480b5e9f3f6fe745d369a53a11451bc307862fa99945ecfd"
NEW_ID = "999999995"
NEW_NAME = "Roundabout RU"
HEADER_SIZE = 168
DESCRIPTOR_SIZE = 176


def require(value, message):
    if not value:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def rgba(path):
    with Image.open(path) as im:
        im = im.convert("RGBA")
        return im.size, im.tobytes()


def decode_frame(frame, width, height):
    require(len(frame) >= 8 and frame[:4] == bytes.fromhex("e021a55a"),
            "Unknown RLE frame magic")
    parameter = struct.unpack_from("<I", frame, 4)[0]
    expected = 1024 + width * height
    require(parameter & 15 == 1 and parameter >> 4 == expected,
            "Unexpected indexed8 RLE parameter")
    raw = bytearray()
    p = 8
    while p < len(frame):
        tag = frame[p]
        p += 1
        count = tag & 127
        require(count and len(raw) + count <= expected, "Invalid RLE run")
        if tag & 128:
            require(p + count <= len(frame), "Truncated RLE literal")
            raw.extend(frame[p:p + count])
            p += count
        else:
            require(p < len(frame), "Truncated RLE repeat")
            raw.extend(frame[p:p + 1] * count)
            p += 1
    require(len(raw) == expected, "Incorrect decoded frame length")
    # The 256-entry palette is BGRA; PNG/Pillow pixels are RGBA.
    palette = [bytes((raw[4*i + 2], raw[4*i + 1], raw[4*i], raw[4*i + 3]))
               for i in range(256)]
    return b"".join(palette[i] for i in raw[1024:])


def encode_frame(pixels, width, height):
    require(len(pixels) == width * height * 4, "Incorrect RGBA input size")
    palette = {}
    indices = bytearray()
    for p in range(0, len(pixels), 4):
        color = pixels[p:p + 4]
        if color not in palette:
            require(len(palette) < 256, "Edited image exceeds indexed8 palette")
            palette[color] = len(palette)
        indices.append(palette[color])
    raw = bytearray()
    for r, g, b, a in palette:
        raw.extend((b, g, r, a))
    raw.extend(bytes(1024 - len(raw)))
    raw.extend(indices)
    result = bytearray(bytes.fromhex("e021a55a"))
    result.extend(struct.pack("<I", (len(raw) << 4) | 1))
    p = 0
    while p < len(raw):
        run = 1
        while p + run < len(raw) and run < 127 and raw[p + run] == raw[p]:
            run += 1
        if run >= 3:
            result.extend((run, raw[p]))
            p += run
        else:
            start = p
            p += 1
            while p < len(raw) and p - start < 127:
                if p + 2 < len(raw) and raw[p] == raw[p + 1] == raw[p + 2]:
                    break
                p += 1
            result.append(128 | (p - start))
            result.extend(raw[start:p])
    result = bytes(result)
    require(decode_frame(result, width, height) == pixels,
            "Encoded indexed8 frame does not round-trip exactly")
    return result


def parse_image(data, kind):
    require(len(data) >= 12, "Truncated image resource")
    a, b, c, d, width, height, total = struct.unpack_from("<BBBBHHI", data)
    require(a == 16 and d == 0 and width and height, "Unknown image header")
    if kind == 2:
        require((b, c) == (4, 0), "Not an indexed8 single image")
        sizes = [total]
        p = 12
    else:
        require(kind == 3 and b and c == 4, "Not an indexed8 image array")
        require(len(data) >= 12 + b * 4, "Truncated array frame sizes")
        sizes = list(struct.unpack_from("<" + "I" * b, data, 12))
        require(sum(sizes) == total, "Array length mismatch")
        p = 12 + b * 4
    require(p + total == len(data), "Image resource length mismatch")
    frames = []
    for size in sizes:
        require(size >= 8, "Truncated frame")
        frames.append(data[p:p + size])
        p += size
    return (width, height), frames


def rebuild_image(kind, size, frames):
    width, height = size
    lengths = [len(f) for f in frames]
    if kind == 2:
        require(len(frames) == 1, "Single image has multiple frames")
        header = struct.pack("<BBBBHHI", 16, 4, 0, 0, width, height, sum(lengths))
    else:
        header = struct.pack("<BBBBHHI", 16, len(frames), 4, 0,
                             width, height, sum(lengths))
        header += struct.pack("<" + "I" * len(frames), *lengths)
    return header + b"".join(frames)


def index_entries(data):
    require(data[:4] == bytes.fromhex("5aa53412"), "Wrong face magic")
    require(struct.unpack_from("<I", data, 16)[0] == 0x903,
            "Wrong device format")
    require(data[28] == 8, "Expected eight style descriptors")
    result = []
    for face in range(data[28]):
        base = HEADER_SIZE + face * DESCRIPTOR_SIZE
        for kind in range(12):
            count, offset = struct.unpack_from("<II", data, base + 8 + 8 * kind)
            require(count <= 10000 and offset + 16 * count <= len(data),
                    "Invalid index table range")
            for n in range(count):
                position = offset + n * 16
                rid, reserved, payload, size = struct.unpack_from("<IIII", data, position)
                require(reserved == 0 and rid >> 24 == kind,
                        "Unexpected resource index format")
                require(payload >= HEADER_SIZE + 8 * DESCRIPTOR_SIZE and
                        payload + size <= len(data), "Resource outside face")
                result.append((face, kind, rid, payload, size, position))
    return result


def source_path(root, relative):
    path = (root / "resources" / relative).resolve()
    require(path.is_relative_to((root / "resources").resolve()), "Unsafe image path")
    return path


def manifest_resources(root):
    uidmap = {}
    for line in (root / "uidmap.map").read_text().splitlines():
        name, rid = line.split(": ", 1)
        uidmap[name] = int(rid, 16)
    tree = ET.parse(root / "resources/manifest.xml").getroot()
    resources = {}
    for element in tree.find("Resources"):
        if element.tag not in ("Image", "ImageArray"):
            continue
        name = element.get("name")
        if name not in uidmap:
            continue  # Only original previews lack UIDs; changed files checked below.
        kind = 2 if element.tag == "Image" else 3
        files = ([element.get("src")] if kind == 2 else
                 [child.get("src") for child in element])
        require(all(files), "Missing source image path")
        require(uidmap[name] >> 24 == kind, "UID/source type mismatch")
        resources[uidmap[name]] = (kind, files)
    return tree, resources


def check_manifest_changes(original, localized):
    require(localized.get("id") == NEW_ID and localized.get("name") == "@watchfaceName",
            "Localized ID/name differ from agreed separate copy")
    localized.set("id", original.get("id"))
    old_title = original.find("Resources/Translation[@name='watchfaceName']")
    new_title = localized.find("Resources/Translation[@name='watchfaceName']")
    require(old_title is not None and new_title is not None,
            "Missing localized name translation")
    require(len(old_title) == len(new_title), "Title language count changed")
    for old, new in zip(old_title, new_title):
        require(old.get("language") == new.get("language") and
                new.get("str") == NEW_NAME, "Localized title/language mismatch")
        new.set("str", old.get("str"))
    require(ET.tostring(original) == ET.tostring(localized),
            "Unexpected manifest edits: layouts/actions must remain identical")


def title_block(original):
    require(struct.unpack_from("<III", original, 0x68) == (0xFFFFFFFF, 0x06000000, 0),
            "Unknown title metadata")
    offset, size = struct.unpack_from("<II", original, 0x74)
    old = original[offset:offset + size]
    require(len(old) == size and size >= 8, "Truncated title")
    mask = struct.unpack_from("<Q", old)[0]
    count = bin(mask).count("1")
    require(count == 33, "Unexpected title language mask")
    lengths = struct.unpack_from("<" + "I" * count, old, 8)
    require(8 + count * 4 + sum(lengths) == len(old), "Unexpected title block layout")
    encoded = NEW_NAME.encode("utf-8")
    return struct.pack("<Q", mask) + struct.pack("<" + "I" * count,
                                                *([len(encoded)] * count)) + encoded * count


def preview_mask(original_root, localized_root, theme):
    """Allow only the actual old/new Steps/Calories glyph support, not icons."""
    mask = bytearray(336 * 480)
    for side, source_key in ((-4, "healthStepCount"), (169, "healthCalorieValue")):
        slot = next(s for s in theme["children"] if s["type"] == "Slot" and
                    s["attrs"].get("x") == side)
        widget = next(w for w in slot["children"] if source_key in json.dumps(w))
        element = next(n for n in widget["children"] if n["type"] == "Image")
        filename = next(iter(element["attrs"]["resources"]["pointer0"].values()))[0]
        x = slot["attrs"]["x"] + element["attrs"]["x"]
        y = slot["attrs"]["y"] + element["attrs"]["y"]
        for root in (original_root, localized_root):
            (width, height), pixels = rgba(source_path(root, filename))
            for iy in range(height):
                for ix in range(width):
                    if (side == -4 and ix < 40) or (side == 169 and ix >= 96):
                        continue
                    if 0 <= x + ix < 336 and 0 <= y + iy < 480 and pixels[(iy*width + ix)*4 + 3]:
                        mask[(y + iy)*336 + x + ix] = 1
    return mask


def append_previews(original_root, localized_root, preview_root, original, result, allowed):
    editor = json.loads((original_root / "editor.config.json").read_text())
    themes = [theme for theme in editor["themes"] if theme["type"] == "normal"]
    require(len(themes) == 4, "Expected four normal previews")
    records = []
    for style, (descriptor, theme) in enumerate(zip((0, 2, 3, 4), themes), 1):
        pointer_field = HEADER_SIZE + descriptor * DESCRIPTOR_SIZE + 4
        old_offset = struct.unpack_from("<I", original, pointer_field)[0]
        require(old_offset != 0, "Missing normal style preview")
        length = 12 + struct.unpack_from("<I", original, old_offset + 8)[0]
        size, frames = parse_image(original[old_offset:old_offset + length], 2)
        require(size == (336, 480), "Unexpected thumbnail dimensions")
        old_pixels = decode_frame(frames[0], *size)
        image_path = preview_root / f"style_{style}.png"
        new_size, pixels = rgba(image_path)
        require(new_size == size, "Thumbnail dimensions changed")
        mask = preview_mask(original_root, localized_root, theme)
        changed = 0
        for p in range(0, len(pixels), 4):
            if pixels[p:p + 4] != old_pixels[p:p + 4]:
                require(mask[p // 4], "Thumbnail changed outside label glyphs: " + str(style))
                changed += 1
        require(changed > 0, "Thumbnail did not change")
        # The renderer maps only new glyph pixels to the original palette. This
        # keeps every other compiled pixel exact and avoids whole-image quantizing.
        original_colors = {old_pixels[p:p + 4] for p in range(0, len(old_pixels), 4)}
        require(all(pixels[p:p + 4] in original_colors for p in range(0, len(pixels), 4)),
                "Thumbnail introduces colors outside original compiled palette")
        frame = encode_frame(pixels, *size)
        replacement = rebuild_image(2, size, [frame])
        new_offset = len(result)
        result.extend(replacement)
        struct.pack_into("<I", result, pointer_field, new_offset)
        allowed.update(range(pointer_field, pointer_field + 4))
        if style == 1:
            require(struct.unpack_from("<I", original, 0x20)[0] == old_offset,
                    "Primary preview pointer differs from style 1")
            struct.pack_into("<I", result, 0x20, new_offset)
            allowed.update(range(0x20, 0x24))
        records.append({"style": style, "descriptor": descriptor,
                        "original_offset": old_offset, "original_size": length,
                        "new_offset": new_offset, "new_size": len(replacement),
                        "localized_png_sha256": sha(image_path.read_bytes()),
                        "modified_pixels": changed,
                        "outside_label_pixels_unchanged": True,
                        "uses_original_palette_colors_only": True})
    for descriptor in (1, 5, 6, 7):
        require(struct.unpack_from("<I", result, HEADER_SIZE + descriptor * DESCRIPTOR_SIZE + 4)[0] == 0,
                "AOD preview unexpectedly added")
    return records


def pack(original_root, localized_root, map_path, preview_root, output, report_path):
    require(output.resolve() != (original_root / "resource.bin").resolve(),
            "Must not overwrite the original")
    original = (original_root / "resource.bin").read_bytes()
    require(sha(original) == ORIGINAL_SHA256, "Original Roundabout fingerprint mismatch")
    mapping = json.loads(map_path.read_text())
    require((mapping["face_name"], mapping["face_id"]) == (NEW_NAME, NEW_ID),
            "Localization map metadata mismatch")
    changes = mapping["images"]
    require(changes and mapping["changed_images"] == len(changes), "Empty/invalid change map")
    old_tree, old_resources = manifest_resources(original_root)
    new_tree, new_resources = manifest_resources(localized_root)
    require(old_resources == new_resources, "Image references/order changed")
    check_manifest_changes(old_tree, new_tree)
    entries = index_entries(original)
    image_entries = {}
    for entry in entries:
        if entry[1] in (2, 3):
            image_entries.setdefault(entry[2], []).append(entry)
    expected_changes = set(changes)
    actual_changes = set()
    for path in (original_root / "resources").rglob("*.png"):
        relative = path.relative_to(original_root / "resources").as_posix()
        edited = source_path(localized_root, relative)
        require(edited.is_file(), "Source PNG missing: " + relative)
        if path.read_bytes() != edited.read_bytes():
            actual_changes.add(relative)
    require(actual_changes == expected_changes, "Edited PNG set differs from audited map")
    for relative, info in changes.items():
        require(sha(source_path(original_root, relative).read_bytes()) == info["original_sha256"],
                "Original PNG hash mismatch: " + relative)
        require(sha(source_path(localized_root, relative).read_bytes()) == info["localized_sha256"],
                "Localized PNG hash mismatch: " + relative)

    result = bytearray(original)
    allowed = set(range(0x28, 0x68)) | set(range(0x74, 0x7C))
    result[0x28:0x68] = NEW_ID.encode("ascii").ljust(64, b"\0")
    seen = set()
    records = []
    original_max_difference = 0
    modified_frames = 0
    for rid, (kind, files) in sorted(old_resources.items()):
        if not expected_changes.intersection(files):
            continue
        require(rid in image_entries, "Edited image UID not found in compiled face")
        references = image_entries[rid]
        offset, length = references[0][3:5]
        require(all(e[3:5] == (offset, length) for e in references),
                "Same image UID refers to multiple payloads")
        resource = original[offset:offset + length]
        size, frames = parse_image(resource, kind)
        require(len(frames) == len(files), "Source/binary frame count mismatch")
        new_frames = []
        changed_indices = []
        for n, (filename, frame) in enumerate(zip(files, frames)):
            if filename not in expected_changes:
                new_frames.append(frame)
                continue
            original_size, old_pixels = rgba(source_path(original_root, filename))
            new_size, pixels = rgba(source_path(localized_root, filename))
            require(size == original_size == new_size == tuple(changes[filename]["size"]),
                    "Image dimensions changed: " + filename)
            decoded = decode_frame(frame, *size)
            # The original compiler slightly quantizes indexed8 colors. Check
            # the source mapping against every visible pixel, with that measured
            # tolerance, before replacing anything. Transparent RGB is immaterial.
            for p in range(0, len(decoded), 4):
                if decoded[p + 3] or old_pixels[p + 3]:
                    difference = max(abs(decoded[p + c] - old_pixels[p + c]) for c in range(4))
                    original_max_difference = max(original_max_difference, difference)
                    require(difference <= 5 and abs(decoded[p + 3] - old_pixels[p + 3]) <= 1,
                            "Compiled/source pixel mismatch: " + filename)
            new_frames.append(encode_frame(pixels, *size))
            seen.add(filename)
            changed_indices.append(n)
            modified_frames += 1
        replacement = rebuild_image(kind, size, new_frames)
        new_offset = len(result)
        result.extend(replacement)
        for entry in references:
            position = entry[5] + 8
            result[position:position + 8] = struct.pack("<II", new_offset, len(replacement))
            allowed.update(range(position, position + 8))
        decoded_size, check_frames = parse_image(replacement, kind)
        require(decoded_size == size and check_frames == new_frames,
                "Rebuilt resource/frame boundaries changed")
        for n in range(len(frames)):
            if n not in changed_indices:
                require(check_frames[n] == frames[n], "Unchanged frame bytes modified")
        records.append({"id": f"0x{rid:08x}", "kind": kind,
                        "original_offset": offset, "original_size": length,
                        "new_offset": new_offset, "new_size": len(replacement),
                        "size": list(size), "changed_frames": changed_indices,
                        "source_files": [files[n] for n in changed_indices],
                        "index_references": len(references),
                        "sha256": sha(replacement)})
    require(seen == expected_changes, "Changed PNGs not covered by compiled resources")
    preview_records = append_previews(original_root, localized_root, preview_root,
                                      original, result, allowed)
    new_title = title_block(original)
    new_title_offset = len(result)
    result.extend(new_title)
    struct.pack_into("<II", result, 0x74, new_title_offset, len(new_title))
    changed_prefix = {i for i, (a, b) in enumerate(zip(original, result)) if a != b}
    require(changed_prefix <= allowed, "Unexpected original-prefix byte modification")
    new_entries = index_entries(result)
    require(len(entries) == len(new_entries), "Resource index count changed")
    modified_ids = {int(r["id"], 16) for r in records}
    unchanged_count = 0
    for old, new in zip(entries, new_entries):
        require(old[:3] == new[:3] and old[5] == new[5], "Resource structure changed")
        if old[2] not in modified_ids:
            require(old == new and original[old[3]:old[3] + old[4]] ==
                    result[new[3]:new[3] + new[4]], "Unchanged resource modified")
            unchanged_count += 1
    require((original_root / "resource.bin").read_bytes() == original,
            "Original was modified during pack")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(result)
    require(output.read_bytes() == result, "Output write verification failed")
    report = {
        "status": "compiled and byte-validated; installation/hardware pending",
        "method": "append-only indexed8 BGRA/RLE resource replacement",
        "name": NEW_NAME, "id": NEW_ID, "original_id": "120917397222",
        "original_sha256": sha(original), "original_bytes": len(original),
        "output": str(output.resolve()), "output_sha256": sha(result),
        "output_bytes": len(result), "added_bytes": len(result) - len(original),
        "map_sha256": sha(map_path.read_bytes()),
        "changed_pngs": len(seen), "modified_frame_instances": modified_frames,
        "modified_image_resources": len(records),
        "unchanged_resource_index_entries": unchanged_count,
        "changed_original_prefix_bytes": len(changed_prefix),
        "allowed_prefix_changes": "64-byte ID; title offset/length; modified image index offset/length; primary/four style preview offsets",
        "original_source_pixel_max_difference": original_max_difference,
        "new_frame_pixel_verification": "exact RGBA equality to each localized PNG",
        "title_languages": 33, "new_title_offset": new_title_offset,
        "new_title_bytes": len(new_title),
        "original_payloads_preserved": True,
        "layouts_actions_preserved": True,
        "descriptor_changes_preview_pointers_only": True,
        "thumbnails": preview_records,
        "records": records,
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "records"},
                     ensure_ascii=False, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "build/roundabout-source")
    parser.add_argument("--localized", type=Path, default=ROOT / "build/roundabout-ru-source")
    parser.add_argument("--map", type=Path, default=ROOT / "tags/roundabout/localization-map.json")
    parser.add_argument("--previews", type=Path, default=ROOT / "build/roundabout-ru-preview-thumbnails")
    parser.add_argument("--output", type=Path, default=ROOT / "build/roundabout-ru.face")
    parser.add_argument("--report", type=Path, default=ROOT / "tags/roundabout/pack_validation.json")
    args = parser.parse_args()
    pack(args.original.resolve(), args.localized.resolve(), args.map.resolve(), args.previews.resolve(),
         args.output.resolve(), args.report.resolve())


if __name__ == "__main__":
    main()
