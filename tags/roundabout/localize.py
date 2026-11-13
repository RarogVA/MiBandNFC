#!/usr/bin/env python3
"""Create a Russian image-resource variant of the user's exact Roundabout.

Never uploads a face. Input is immutable. The compiled resource.bin is deliberately
excluded: use pack tools to build and validate a new binary from changed assets.
"""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import zipfile
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw, ImageFont

DEFAULT_FONT = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
EXPECTED_MANIFEST = "94d047396644ff5e72d8d53995d603a76e128885bbe886837a1f567141821f2c"
FACE_NAME = "Roundabout RU"
FACE_ID = "999999995"
SCALE = 4


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def walk(node):
    yield node
    for child in node.get("children", []):
        yield from walk(child)


def discover(source):
    editor = json.loads((source / "editor.config.json").read_text())
    replacements = {}
    for theme in editor["themes"]:
        for slot in theme["children"]:
            if slot["type"] != "Slot" or slot["attrs"].get("y") != 364:
                continue
            side = "left" if slot["attrs"]["x"] == -4 else "right"
            for widget in slot["children"]:
                sources = {n.get("attrs", {}).get("source") for n in walk(widget)}
                sources.update(value.get("value") for n in walk(widget)
                               for value in n.get("attrs", {}).get("strings", []) if value.get("isSource"))
                translations = {"healthStepCount": "Шаги", "healthHeartRate": "ЧСС",
                                "healthCalorieValue": "Ккал", "healthExerciseDuration": "Активность",
                                "healthStandCount": "Стоя"}
                text = next((label for key, label in translations.items() if key in sources), None)
                if text is None:
                    continue
                for child in widget.get("children", []):
                    if child["type"] != "Image":
                        continue
                    for files in child["attrs"].get("resources", {}).get("pointer0", {}).values():
                        for file in files:
                            replacements[file] = dict(text=text, kind="arc_label", side=side)
    root = ET.parse(source / "resources/manifest.xml").getroot()
    resources = {n.get("name"): n for n in root.find("Resources")}
    # The original index 0 has visually verified SUN; retain its Sunday-first order.
    russian_days = ["ВС", "ПН", "ВТ", "СР", "ЧТ", "ПТ", "СБ"]
    for element in resources.values():
        if element.tag == "DataItemImageValues" and element.get("source") == "dateWeek":
            array = resources[element.get("ref")[1:]]
            assert len(array) == 7
            for item, text in zip(array, russian_days):
                key = item.get("src")
                value = dict(text=text, kind="weekday")
                if key in replacements and replacements[key] != value:
                    raise ValueError("Conflicting day image reuse")
                replacements[key] = value
        if element.tag == "DataItemImageNumber" and element.get("source") == "healthSleepDuration":
            image = resources[element.get("unitIcon")[1:]]
            replacements[image.get("src")] = dict(text="ч.", kind="sleep_unit")
    return replacements


def typography(text, font, color, target_width, target_height, angle=0, max_size=17):
    """New glyph raster, with integer pixel output and no runtime font dependency."""
    for size in range(max_size * SCALE, 5 * SCALE - 1, -1):
        face = ImageFont.truetype(str(font), size)
        bbox = face.getbbox(text)
        image = Image.new("L", (max(1, bbox[2]-bbox[0]), max(1, bbox[3]-bbox[1])))
        ImageDraw.Draw(image).text((-bbox[0], -bbox[1]), text, font=face, fill=255)
        if angle:
            image = image.rotate(angle, resample=Image.Resampling.BICUBIC, expand=True)
        if image.width <= target_width * SCALE and image.height <= target_height * SCALE:
            mask = image.resize((max(1, round(image.width/SCALE)), max(1, round(image.height/SCALE))), Image.Resampling.LANCZOS)
            glyph = Image.new("RGBA", mask.size, (*color, 255))
            glyph.putalpha(mask)
            return glyph
    raise ValueError("Translation cannot fit")


def translate_image(original, spec, font):
    out = original.convert("RGBA")
    width, height = out.size
    if spec["kind"] == "arc_label":
        region = (40, 0, width, height) if spec["side"] == "left" else (0, 0, 96, height)
        angle = -32 if spec["side"] == "left" else 32
    else:
        region = (0, 0, width, height)
        angle = 0
    fragment = out.crop(region)
    bbox = fragment.getchannel("A").getbbox()
    if not bbox:
        raise ValueError("No original text in expected region")
    box = (bbox[0] + region[0], bbox[1] + region[1], bbox[2] + region[0], bbox[3] + region[1])
    colors = Counter(tuple(p[:3]) for p in fragment.getdata() if p[3] > 220)
    if not colors:
        raise ValueError("No opaque source text color")
    color = colors.most_common(1)[0][0]
    # For suffix/day labels use the whole fixed canvas; arc labels retain their
    # original occupied region and preserve the separate icon outside it.
    target = box if spec["kind"] == "arc_label" else (0, 0, width, height)
    if spec["kind"] == "sleep_unit":
        # Two Cyrillic characters share the former one-letter h slot. Preserve
        # its height/readability by condensing horizontally inside that slot.
        glyph = typography(spec["text"], font, color, width*2, height, max_size=22)
        if glyph.width > width:
            glyph = glyph.resize((width, glyph.height), Image.Resampling.LANCZOS)
    else:
        glyph = typography(spec["text"], font, color, target[2]-target[0], target[3]-target[1], angle)
    out.paste((0, 0, 0, 0), region)
    x = target[0] + (target[2]-target[0]-glyph.width)//2
    y = target[1] + (target[3]-target[1]-glyph.height)//2
    out.alpha_composite(glyph, (x, y))
    # The compiled watchface uses an indexed8 palette. Reduce only the new glyph
    # alpha precision if needed, preserving original icons exactly.
    for levels in (32, 16, 8, 4, 2):
        if len(set(out.getdata())) <= 256:
            break
        alpha = glyph.getchannel("A").point(lambda value: round(round(value*(levels-1)/255)*255/(levels-1)))
        glyph.putalpha(alpha)
        out.paste((0, 0, 0, 0), region)
        out.alpha_composite(glyph, (x, y))
    if len(set(out.getdata())) > 256:
        raise ValueError("Translated image cannot fit the indexed8 palette without changing its icon")
    if spec["kind"] == "arc_label":
        untouched = (0, 0, 40, height) if spec["side"] == "left" else (96, 0, width, height)
        assert out.crop(untouched).tobytes() == original.convert("RGBA").crop(untouched).tobytes()
    assert out.size == original.size
    return out, dict(color=list(color), region=list(region), old_text_box=list(box), new_text_box=[x, y, x+glyph.width, y+glyph.height])


def rename_variant(destination):
    description = destination / "description.xml"
    text = description.read_text()
    text = re.sub(r"<name>.*?</name>", f"<name>{FACE_NAME}</name>", text, count=1)
    text = re.sub(r"<pkgName>.*?</pkgName>", f"<pkgName>{FACE_ID}</pkgName>", text, count=1)
    description.write_text(text)
    editor_path = destination / "editor.config.json"
    editor = json.loads(editor_path.read_text())
    names = editor["i18n"]["translations"]["watchfaceName"]
    for locale in names:
        names[locale] = FACE_NAME
    editor_path.write_text(json.dumps(editor, ensure_ascii=False, indent=2)+"\n")
    manifest_path = destination / "resources/manifest.xml"
    text = manifest_path.read_text().replace('id="120917397222"', f'id="{FACE_ID}"', 1)
    text, count = re.subn(r'(<Translation name="watchfaceName">)(.*?)(</Translation>)',
                         lambda match: match[1] + re.sub(r'str="[^"]*"', f'str="{FACE_NAME}"', match[2]) + match[3],
                         text, count=1, flags=re.DOTALL)
    assert count == 1
    manifest_path.write_text(text)


def extract_original(archive):
    """Extract a verified-path local archive into ignored project build storage."""
    root = Path(__file__).resolve().parents[2]
    destination = root / "build" / ("roundabout-input-" + sha(archive)[:12])
    with zipfile.ZipFile(archive) as zf:
        names = zf.namelist()
        if len(names) != len(set(names)):
            raise ValueError("Duplicate paths in source archive")
        for entry in zf.infolist():
            parts = Path(entry.filename)
            if parts.is_absolute() or ".." in parts.parts or (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError("Unsafe source archive entry")
        destination.mkdir(parents=True, exist_ok=True)
        for entry in zf.infolist():
            target = destination / entry.filename
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            data = zf.read(entry)
            if target.exists() and target.read_bytes() != data:
                raise ValueError("Existing extracted original differs from archive")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    return destination


def preview_sheet(source, destination, specs, target):
    # This is an asset comparison, not a claim of a device-rendered screenshot.
    columns = 3
    rows = (len(specs) + columns - 1)//columns
    image = Image.new("RGB", (columns*360, rows*100), "#16191f")
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype(DEFAULT_FONT, 12)
    for i, (file, spec) in enumerate(sorted(specs.items())):
        x, y = (i % columns)*360, (i//columns)*100
        before = Image.open(source / "resources" / file).convert("RGBA")
        after = Image.open(destination / "resources" / file).convert("RGBA")
        # Light patches make the black-label styles visible too.
        bg = "#f3ddce" if spec.get("render", {}).get("color") == [0, 0, 0] else "#000000"
        draw.rectangle((x+4, y+22, x+350, y+97), fill=bg)
        image.paste(before, (x+12, y+29), before)
        image.paste(after, (x+194, y+29), after)
        draw.text((x+5, y+4), f"{spec['text']}   {file[:35]}", font=font, fill="#ffffff")
    image.save(target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--font", type=Path, default=Path(DEFAULT_FONT))
    parser.add_argument("--source-zip", action="store_true", help="Treat source as the saved original ZIP; extract under ignored build/")
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--preview", type=Path, required=True)
    args = parser.parse_args()
    if args.source_zip:
        args.source = extract_original(args.source)
    if sha(args.source / "resources/manifest.xml") != EXPECTED_MANIFEST:
        raise ValueError("Not the exact Roundabout 1.1.20 source")
    if args.destination.exists():
        raise ValueError("Destination must not exist; original and previous outputs are preserved")
    if not args.font.is_file():
        raise ValueError("A Cyrillic-capable TTF is required")
    specs = discover(args.source)
    shutil.copytree(args.source, args.destination, ignore=shutil.ignore_patterns("resource.bin", "hashCode"))
    for file, spec in sorted(specs.items()):
        original_path = args.source / "resources" / file
        out_path = args.destination / "resources" / file
        original = Image.open(original_path)
        out, evidence = translate_image(original, spec, args.font)
        out.save(out_path)
        spec.update(size=list(original.size), render=evidence, original_sha256=sha(original_path), localized_sha256=sha(out_path))
    for name in ("resources/manifest.xml", "editor.config.json", "description.xml", "capability.json", "uidmap.map"):
        assert sha(args.source / name) == sha(args.destination / name)
    rename_variant(args.destination)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = dict(status="localized source assets; not an installable face", source_manifest_sha256=EXPECTED_MANIFEST,
                  font_name=args.font.name, font_sha256=sha(args.font), glyph_scale=SCALE,
                  face_name=FACE_NAME, face_id=FACE_ID, changed_images=len(specs), images=specs,
                  weekday_index_order=["ВС", "ПН", "ВТ", "СР", "ЧТ", "ПТ", "СБ"],
                  layout_unchanged=True, action_keys_unchanged=True, numeric_data_sources_unchanged=True,
                  compiled_binary_included=False, hardware_changed=False)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n")
    args.preview.parent.mkdir(parents=True, exist_ok=True)
    preview_sheet(args.source, args.destination, specs, args.preview)
    print(json.dumps(dict(changed_images=len(specs), source=str(args.destination), preview=str(args.preview)), ensure_ascii=False))


if __name__ == "__main__":
    main()
