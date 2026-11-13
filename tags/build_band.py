#!/usr/bin/env python3
"""Build the Type 4 tag bridge for p67glt / 3.201.026; never install it.

Outputs remain under tags/build. The original APDU Lab releases and template
are immutable inputs. A successful build does not establish hardware support.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from build_support import android_sdk
from package_face import EXPECTED as TEMPLATE_SHA256, build as package_face, records

OUT = ROOT / "tags/build"
OUT.mkdir(parents=True, exist_ok=True)
NDK = Path(os.environ.get("APDU_NDK") or os.environ.get("ANDROID_NDK_HOME") or
           str(android_sdk() / "ndk/28.2.13676358")).expanduser()
host = {"darwin": "darwin-x86_64", "linux": "linux-x86_64", "win32": "windows-x86_64"}.get(sys.platform)
if not host:
    raise SystemExit("Unsupported NDK host: " + sys.platform)
BIN = NDK / "toolchains/llvm/prebuilt" / host / "bin"
if not (BIN / "clang").exists() and not (BIN / "clang.exe").exists():
    raise SystemExit("NDK clang not found. Set APDU_NDK to a local NDK directory: " + str(NDK))

SOURCES = [
    "tags/band/module.c", "tags/band/tag_record.c", "tags/band/tag_display.c",
    "band/transport.c", "band/native_clock.c", "band/nfc_probe.c",
    "band/route_table.c", "band/runtime.c",
]
FLAGS = [
    "--target=arm-none-eabi", "-mcpu=cortex-m33", "-mthumb", "-mfloat-abi=hard",
    "-mfpu=fpv5-sp-d16", "-mno-movt", "-Oz", "-g0", "-std=c11", "-Wall", "-Wextra",
    "-Werror", "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
    "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
    "-ffunction-sections", "-fdata-sections",
    "-DNFC_PROBE_ENABLE_EXPERIMENTAL_ROUTING=1", "-DTRANSPORT_TAG_PROFILE=1",
]


def run(args):
    result = subprocess.run([str(arg) for arg in args], capture_output=True, text=True, cwd=ROOT)
    if result.returncode:
        print(result.stdout + result.stderr, file=sys.stderr)
        result.check_returncode()
    return result.stdout


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def check_elf(data):
    """Verify the firmware loader's constructor, not just ELF e_entry."""
    if data[:7] != b"\x7fELF\x01\x01\x01" or struct.unpack_from("<HH", data, 16) != (1, 40):
        raise RuntimeError("Unexpected ELF ABI")
    entry = struct.unpack_from("<I", data, 24)[0]
    if not entry or not entry & 1:
        raise RuntimeError("Missing Thumb entry")
    shoff = struct.unpack_from("<I", data, 32)[0]
    shsize, shcount = struct.unpack_from("<HH", data, 46)
    if shsize != 40 or shoff + shcount * shsize > len(data):
        raise RuntimeError("Unexpected section ABI")
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shcount)]
    ctors = [(i, section) for i, section in enumerate(sections) if section[1] == 14]
    if len(ctors) != 1 or ctors[0][1][5] != 4 or not ctors[0][1][2] & 2:
        raise RuntimeError("Need exactly one allocated 4-byte constructor array")
    ctor_index, ctor_section = ctors[0]
    rels = [section for section in sections if section[1] == 9 and section[7] == ctor_index]
    if len(rels) != 1 or rels[0][5] != 8:
        raise RuntimeError("Need one constructor relocation")
    rel = rels[0]
    reloc_offset, info = struct.unpack_from("<II", data, rel[4])
    if reloc_offset != 0 or info & 255 not in (2, 38):
        raise RuntimeError("Unexpected constructor relocation")
    symsec = sections[rel[6]]
    strings = sections[symsec[6]]

    def symbol(index):
        name_index, value, size, kind, other, section = struct.unpack_from(
            "<IIIBBH", data, symsec[4] + index * 16)
        start = strings[4] + name_index
        name = data[start:data.index(b"\0", start)].decode()
        return name, value, section

    symbols = [symbol(i) for i in range(symsec[5] // 16)]
    wrapper = [symbol for symbol in symbols if symbol[0] == "module_constructor"]
    if len(wrapper) != 1 or not wrapper[0][1] & 1:
        raise RuntimeError("Missing Thumb constructor")
    target = symbol(info >> 8)
    addend = struct.unpack_from("<I", data, ctor_section[4])[0]
    pointer = (target[1] + addend) & 0xFFFFFFFF
    if target[2] != wrapper[0][2] or pointer != wrapper[0][1]:
        raise RuntimeError("Constructor relocation does not target module_constructor")
    return entry, dict(section_type="SHT_INIT_ARRAY", bytes=4,
                       relocation_type=info & 255, target=wrapper[0][0], pointer=hex(pointer))


def elf_semantics(data):
    """Normalize allocated bytes and relocation targets across symbol stripping."""
    shoff = struct.unpack_from("<I", data, 32)[0]
    shsize, shcount, names_index = struct.unpack_from("<HHH", data, 46)
    if shsize != 40:
        raise RuntimeError("Unexpected section ABI while checking stripped ELF")
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shcount)]
    name_section = sections[names_index]

    def name(section):
        start = name_section[4] + section[0]
        return data[start:data.index(b"\0", start)].decode()

    allocated = {}
    relocations = []
    for section in sections:
        if section[2] & 2:
            # NOBITS allocates zero-initialized storage but has no file bytes.
            content = b"" if section[1] == 8 else data[section[4]:section[4] + section[5]]
            allocated[name(section)] = (section[1], section[2], section[3], section[5], section[8], content)
        if section[1] not in (4, 9):
            continue
        symtab = sections[section[6]]
        width = 12 if section[1] == 4 else 8
        if section[9] != width or section[5] % width:
            raise RuntimeError("Unexpected relocation entry size")
        for at in range(section[4], section[4] + section[5], width):
            offset, info = struct.unpack_from("<II", data, at)
            _, value, size, kind, other, target_index = struct.unpack_from(
                "<IIIBBH", data, symtab[4] + (info >> 8) * 16)
            target = name(sections[target_index]) if 0 < target_index < shcount else target_index
            addend = struct.unpack_from("<i", data, at + 8)[0] if width == 12 else None
            relocations.append((name(sections[section[7]]), offset, info & 255,
                                value, size, kind, other, target, addend))
    return allocated, relocations


def build_face(lua, elf, diagnostic_v3=False):
    template = (ROOT / "references/recorder.face").read_bytes()
    packed, entries = package_face(template, lua, elf)
    data = bytearray(packed)
    title_width = len(b"Recorder Service Installer")
    if data[0x28:0x31] != b"999999997":
        raise RuntimeError("Unexpected APDU packer ID")
    if data[0x68:0x68 + title_width] != b"APDU Lab Installer".ljust(title_width, b"\0"):
        raise RuntimeError("Unexpected APDU packer title")
    # Keep the verified fixed-size fields and resource table layout.
    data[0x28:0x31] = b"999999996"
    data[0x68:0x68 + title_width] = b"Tag Bridge Installer".ljust(title_width, b"\0")
    original_elf_name = "app/lua/apdu_lab_probe_3.201.026.elf"
    unique_elf_name = "app/lua/tag_bridge_v03_3.201.026.elf"
    for item in entries:
        if diagnostic_v3 and item["name"] == original_elf_name:
            if len(unique_elf_name.encode()) != item["namesize"]:
                raise RuntimeError("Unique ELF name changed template field width")
            data[item["name_at"]:item["start"]] = unique_elf_name.encode()
    parsed = records(data)
    expected_entries = [dict(item, name=unique_elf_name) if diagnostic_v3 and item["name"] == original_elf_name
                        else item for item in entries]
    if len(data) != len(template) or parsed != expected_entries:
        raise RuntimeError("Tag FACE resource layout changed")
    for item in parsed:
        payload = data[item["start"]:item["end"]]
        if item["name"] == "app/lua/main.lua" and payload != lua.ljust(item["length"], b" "):
            raise RuntimeError("Tag Lua round trip failed")
        if item["name"].endswith(".elf") and payload != elf.ljust(item["length"], b"\0"):
            raise RuntimeError("Tag ELF round trip failed")
    return bytes(data), parsed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--diagnostic-v3", action="store_true",
                        help="Build the unverified v3 diagnostic candidate in separate outputs; default reproduces hardware-tested v2")
    args = parser.parse_args()
    suffix = "-v3" if args.diagnostic_v3 else ""
    flags = FLAGS + (["-DTAG_DIAGNOSTIC_V3=1",
                      '-DDIAGNOSTIC_STATE_PATH="/tmp/tag_catalog_diag"'] if args.diagnostic_v3 else [])
    lua_path = ROOT / ("tags/watchface/main-v3.lua" if args.diagnostic_v3 else "tags/watchface/main.lua")
    objects = []
    for source in SOURCES:
        path = ROOT / source
        if not path.is_file():
            raise SystemExit("Missing native source: " + source)
        obj = OUT / (source.replace("/", "_") + suffix + ".o")
        run([BIN / "clang", *flags, "-I", ROOT / "band", "-I", ROOT / "tags/band", "-c", path, "-o", obj])
        run([BIN / "llvm-objcopy", "--remove-section=.ARM.exidx*", "--remove-section=.comment", obj])
        objects.append(obj)
    elf_path = OUT / ("tag-bridge" + suffix + "-p67glt-3.201.026.elf")
    run([BIN / "ld.lld", "-r", "-T", ROOT / "band/module.ld", "-o", elf_path, *objects])
    unstripped = elf_path.read_bytes()
    check_elf(unstripped)
    # The firmware needs allocated bytes and their relocation targets, not
    # unused local symbols. Keep the constructor name for an independent
    # post-strip check and verify that loaded bytes/relocation meaning survive.
    run([BIN / "llvm-objcopy", "--strip-unneeded", "--keep-symbol=module_constructor", elf_path])
    if elf_semantics(unstripped) != elf_semantics(elf_path.read_bytes()):
        raise RuntimeError("Symbol stripping changed loaded bytes or relocation semantics")
    undefined = run([BIN / "llvm-nm", "--undefined-only", elf_path]).strip()
    if undefined:
        raise RuntimeError("Unresolved symbols: " + undefined)
    elf = elf_path.read_bytes()
    entry, constructor = check_elf(elf)
    if len(elf) > 31272:
        raise RuntimeError(f"ELF exceeds verified template slot: {len(elf)} > 31272")
    lua = lua_path.read_bytes()
    face, entries = build_face(lua, elf, args.diagnostic_v3)
    face_path = OUT / ("tag-bridge" + suffix + "-p67glt-3.201.026.face")
    face_path.write_bytes(face)
    (OUT / ("band-elf-inspection" + suffix + ".txt")).write_text(run([BIN / "llvm-readelf", "-h", "-A", "-r", "-S", elf_path]))
    source_files = set(SOURCES)
    source_files.update(str(p.relative_to(ROOT)) for base in ("band", "tags/band")
                        for p in (ROOT / base).glob("*.h"))
    source_files.update(("band/diagnostic.c", "band/module.ld", str(lua_path.relative_to(ROOT)), "tags/build_band.py"))
    report = dict(
        model="miwear.watch.p67glt", firmware="3.201.026", profile="NDEF Type 4 tag bridge",
        compiler=run([BIN / "clang", "--version"]).splitlines()[0], flags=flags,
        variant="diagnostic-v3 (hardware unverified)" if args.diagnostic_v3 else "v2 (hardware-tested artifact reproduction)",
        elf=dict(path=str(elf_path.relative_to(ROOT)), bytes=len(elf), sha256=sha256(elf),
                 entry=hex(entry), resource_bytes=31272, spare_bytes=31272 - len(elf),
                 unstripped_bytes=len(unstripped), symbol_strip_preserved_loaded_bytes_and_relocations=True,
                 undefined_symbols=[], constructor=constructor),
        face=dict(path=str(face_path.relative_to(ROOT)), bytes=len(face), sha256=sha256(face),
                  id="999999996", lua_bytes=len(lua), lua_resource_bytes=4529,
                  diagnostic_version=3 if args.diagnostic_v3 else 2,
                  diagnostic_path="/tmp/tag_catalog_diag" if args.diagnostic_v3 else "/tmp/apdu_lab_state",
                  elf_resource="app/lua/tag_bridge_v03_3.201.026.elf" if args.diagnostic_v3 else "app/lua/apdu_lab_probe_3.201.026.elf",
                  template_sha256=TEMPLATE_SHA256, records=entries),
        source_sha256={source: sha256((ROOT / source).read_bytes()) for source in sorted(source_files)},
        hardware_tested=False,
        notes=["Exact template resource layout retained; preview assets remain from Recorder.",
               "Build validates ARM ABI, firmware-loader constructor, complete symbols, slot size and FACE payloads.",
               "No device connected, installed or executed by this script."],
    )
    (OUT / ("band-build" + suffix + ".json")).write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ("profile", "elf", "hardware_tested")}, indent=2))


if __name__ == "__main__":
    main()
