#!/usr/bin/env python3
"""Preview the default four styles using original compiled gallery images.

The default Steps and Calories labels use the actual localized resources.
This is an offline resource preview, not a screenshot from the band.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from PIL import Image

from pack_roundabout import parse_image, decode_frame, HEADER_SIZE, DESCRIPTOR_SIZE

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("localized", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    editor = json.loads((args.source / "editor.config.json").read_text())
    grid = Image.new("RGB", (336*4, 480))
    thumbnail_root = args.output.with_name(args.output.stem + "-thumbnails")
    thumbnail_root.mkdir(parents=True, exist_ok=True)
    original_binary = (args.source / "resource.bin").read_bytes()
    report = []
    for index, theme in enumerate([t for t in editor["themes"] if t["type"] == "normal"]):
        descriptor_index = (0, 2, 3, 4)[index]
        offset = struct.unpack_from("<I", original_binary, HEADER_SIZE + descriptor_index*DESCRIPTOR_SIZE + 4)[0]
        length = 12 + struct.unpack_from("<I", original_binary, offset+8)[0]
        size, frames = parse_image(original_binary[offset:offset+length], 2)
        assert size == (336, 480) and len(frames) == 1
        original_pixels = decode_frame(frames[0], *size)
        original_preview = Image.frombytes("RGBA", size, original_pixels)
        preview = original_preview.copy()
        background = original_preview.getpixel((168, 470))
        assert background[0] == background[1] == background[2] and background[0] in (0, 254, 255) and background[3] >= 254
        allowed = set()
        for side, source_key in ((-4, "healthStepCount"), (169, "healthCalorieValue")):
            slot = next(s for s in theme["children"] if s["type"] == "Slot" and s["attrs"].get("x") == side)
            widget = next(w for w in slot["children"] if source_key in json.dumps(w))
            image = next(n for n in widget["children"] if n["type"] == "Image")
            file = next(iter(image["attrs"]["resources"]["pointer0"].values()))[0]
            x = slot["attrs"]["x"] + image["attrs"]["x"]
            y = slot["attrs"]["y"] + image["attrs"]["y"]
            before = Image.open(args.source / "resources" / file).convert("RGBA")
            after = Image.open(args.localized / "resources" / file).convert("RGBA")
            # The original lettering is over a plain background. Erase only its
            # alpha support; the arc, live numbers and icon remain untouched.
            mask = before.getchannel("A").point(lambda a: 255 if a else 0)
            protected = range(min(40, mask.width)) if side == -4 else range(96, mask.width)
            for yy in range(mask.height):
                for xx in protected:
                    mask.putpixel((xx, yy), 0)
                    after.putpixel((xx, yy), (0, 0, 0, 0))
            for yy in range(mask.height):
                for xx in range(mask.width):
                    if mask.getpixel((xx, yy)) or after.getpixel((xx, yy))[3]:
                        allowed.add((x+xx, y+yy))
            preview.paste(background, (x, y, x+before.width, y+before.height), mask)
            preview.alpha_composite(after, (x, y))
        # Keep the original compiled palette. Only changed lettering pixels are
        # matched to its nearest RGBA entry; all other pixels stay byte-exact.
        palette = sorted(set(original_preview.getdata()))
        nearest = {}
        changed_count = 0
        for y in range(size[1]):
            for x in range(size[0]):
                pixel = preview.getpixel((x, y))
                if pixel == original_preview.getpixel((x, y)):
                    continue
                assert (x, y) in allowed, "Preview changed outside translated lettering"
                if pixel not in nearest:
                    nearest[pixel] = min(palette, key=lambda c: sum((a-b)**2 for a,b in zip(c,pixel)))
                preview.putpixel((x, y), nearest[pixel])
                changed_count += 1
        assert len(set(preview.getdata())) <= 256
        thumbnail = preview
        thumbnail.save(thumbnail_root / f"style_{index+1}.png")
        report.append(dict(style=index+1, original_offset=offset, original_payload_sha256=hashlib.sha256(original_binary[offset:offset+length]).hexdigest(),
                           original_rgba_sha256=hashlib.sha256(original_pixels).hexdigest(), localized_rgba_sha256=hashlib.sha256(thumbnail.tobytes()).hexdigest(),
                           palette_colors=len(palette), changed_pixels=changed_count, changed_outside_labels=0,
                           quantization="nearest original compiled RGBA palette, changed lettering only; no dithering"))
        if index == 0:
            thumbnail.convert("RGB").save(args.output)
        grid.paste(thumbnail.convert("RGB"), (336*index, 0))
    grid.save(args.output.with_name(args.output.stem + "-styles.png"))
    (thumbnail_root / "validation.json").write_text(json.dumps(report, indent=2)+"\n")


if __name__ == "__main__":
    main()
