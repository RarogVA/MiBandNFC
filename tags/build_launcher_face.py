#!/usr/bin/env python3
"""Package the existing tag core and an independent UI module; never install.

The recorder image slot is reused for the second ELF with exactly the same
filename width, record length and offsets. The hardware-tested core is unchanged.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from package_face import build as base_pack, records

CORE_SHA = "91f63ed22532c81ba14c0f4d4e6271ace8b55df0a664c6eec8d706b7788c0673"
FACE_ID = "999999993"


def pack(template, lua, core, launcher, revision=1):
    if revision not in (1, 2):
        raise ValueError("Unknown UI revision")
    if hashlib.sha256(core).hexdigest() != CORE_SHA:
        raise ValueError("Expected the hardware-tested, unchanged tag core v2")
    if len(launcher) > 9841 or launcher[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("Launcher must be ELF32 within the 9841-byte image slot")
    # Reuse the audited Lua/core layout and then replace just one spare image.
    packed, entries = base_pack(template, lua, core)
    data = bytearray(packed)
    slot = [e for e in entries if e["name"] == "app/lua/recorder.bin"]
    if len(slot) != 1 or slot[0]["length"] != 9841:
        raise ValueError("Unexpected second-module slot")
    item = slot[0]
    name = b"app/lua/launcher.elf"
    if len(name) != item["namesize"]:
        raise ValueError("Second module filename changes record width")
    data[item["name_at"]:item["start"]] = name
    data[item["start"]:item["end"]] = launcher.ljust(item["length"], b"\0")
    data[0x28:0x31] = FACE_ID.encode()
    width = len(b"Recorder Service Installer")
    data[0x68:0x68 + width] = f"NFC Tags Setup v{revision}".encode().ljust(width, b"\0")
    parsed = records(data)
    expected = [dict(e, name=name.decode()) if e is item else e for e in entries]
    if len(data) != len(template) or parsed != expected:
        raise ValueError("Installer resource layout changed")
    # Every byte outside the audited body/header changes must remain identical.
    allowed = [(0x28, 0x31), (0x68, 0x68 + width)]
    for e in parsed:
        if e["name"] == "app/lua/main.lua":
            body = lua.ljust(e["length"], b" ")
            allowed.append((e["start"], e["end"]))
        elif e["name"] == "app/lua/apdu_lab_probe_3.201.026.elf":
            body = core.ljust(e["length"], b"\0")
            allowed.append((e["name_at"], e["end"]))
        elif e["name"] == name.decode():
            body = launcher.ljust(e["length"], b"\0")
            allowed.append((e["name_at"], e["end"]))
        else:
            body = template[e["start"]:e["end"]]
        if data[e["start"]:e["end"]] != body:
            raise ValueError("Installer payload mismatch: " + e["name"])
    allowed.sort()
    at = 0
    for start, end in allowed:
        if data[at:start] != template[at:start]:
            raise ValueError("Unexpected change outside fixed slots")
        at = end
    if data[at:] != template[at:]:
        raise ValueError("Unexpected trailing change")
    return bytes(data), parsed


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("launcher", type=Path)
    p.add_argument("--revision", type=int, choices=(1, 2), default=1)
    p.add_argument("--lua", type=Path)
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    if args.lua is None:
        args.lua = ROOT / ("tags/watchface/setup.lua" if args.revision == 1 else "tags/watchface/setup-v2.lua")
    if args.output is None:
        args.output = ROOT / f"tags/build/nfc-tags-setup-v{args.revision}-p67glt-3.201.026.face"
    # tools/ and tags/ both contain build_band.py; never resolve by basename.
    spec = importlib.util.spec_from_file_location("tag_core_builder", ROOT / "tags/build_band.py")
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    launcher = args.launcher.read_bytes()
    _, constructor = builder.check_elf(launcher)
    if f"NFC_TAGS_UI{args.revision}\n".encode() not in launcher:
        raise ValueError("Launcher diagnostic identity does not match installer revision")
    core = (ROOT / "releases/tags/tag-bridge-v2-p67glt-3.201.026.elf").read_bytes()
    lua = args.lua.read_bytes()
    if f"NFC Tags Setup v{args.revision}".encode() not in lua:
        raise ValueError("Lua title does not match installer revision")
    face, entries = pack((ROOT / "references/recorder.face").read_bytes(), lua, core, launcher, args.revision)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(face)
    report = dict(id=FACE_ID, revision=args.revision, bytes=len(face), sha256=hashlib.sha256(face).hexdigest(),
                  core_sha256=CORE_SHA, launcher_sha256=hashlib.sha256(launcher).hexdigest(),
                  launcher_bytes=len(launcher), lua_bytes=len(lua), lua_sha256=hashlib.sha256(lua).hexdigest(), constructor=constructor,
                  records=entries, hardware_tested=False)
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "records"}, indent=2))


if __name__ == "__main__":
    main()
