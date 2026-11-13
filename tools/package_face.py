#!/usr/bin/env python3
"""Build an exact-layout test face from the user-provided installer template.

Only fixed-size header/name fields and embedded Lua/ELF bodies are changed.
Resource lengths/offsets are retained. Lua is space padded, ELF zero padded.
The original executable and both original Lua copies are completely replaced.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

EXPECTED = "1d3be2b0c95c21ffa834485a45e326d0d98eeea54be94a1afd086df454e0901c"
FILES_START = 0x26503

def records(data):
    offset = FILES_START
    items = []
    while offset < len(data):
        if offset+20 > len(data):
            raise ValueError("Truncated file record")
        length = int.from_bytes(data[offset:offset+3], "little")
        namesize = data[offset+3]
        name_at = offset+20
        start = name_at+namesize
        end = start+length
        if not namesize or end > len(data) or any(data[offset+4:name_at]):
            raise ValueError("Unexpected template record")
        field = bytes(data[name_at:start])
        if b"\0" in field:
            raise ValueError("Unexpected null in length-delimited filename")
        items.append(dict(name=field.decode(), offset=offset, name_at=name_at,
                          namesize=namesize, start=start, end=end, length=length))
        offset = end
    return items

def build(template, lua, elf):
    if hashlib.sha256(template).hexdigest() != EXPECTED:
        raise ValueError("Unsupported template hash")
    if len(lua) > 4529 or len(elf) > 31272:
        raise ValueError("Payload exceeds verified fixed-size template slot")
    if elf[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("Expected ELF32 little endian")
    if struct.unpack_from("<HH", elf, 16) != (1, 40):
        raise ValueError("Expected ARM ET_REL module")
    out = bytearray(template)
    entries = records(out)
    count_lua = count_elf = 0
    for entry in entries:
        if entry["name"] == "app/lua/main.lua":
            out[entry["start"]:entry["end"]] = lua.ljust(entry["length"], b" ")
            count_lua += 1
        elif entry["name"] == "app/lua/recorder_probe_3.201.026.elf":
            out[entry["start"]:entry["end"]] = elf.ljust(entry["length"], b"\0")
            name = b"app/lua/apdu_lab_probe_3.201.026.elf"
            if len(name)!=entry["namesize"]:
                raise ValueError("Replacement filename must keep exact byte length")
            out[entry["name_at"]:entry["start"]] = name
            count_elf += 1
    if count_lua != 2 or count_elf != 1:
        raise ValueError("Expected two Lua and one ELF template records")
    if out[0x28:0x31] != b"999999998":
        raise ValueError("Unexpected template ID")
    out[0x28:0x31] = b"999999997"
    title = b"Recorder Service Installer"
    if out[0x68:0x68+len(title)] != title:
        raise ValueError("Unexpected template title")
    out[0x68:0x68+len(title)] = b"APDU Lab Installer".ljust(len(title), b"\0")
    parsed = records(out)
    if len(out) != len(template) or len(parsed) != len(entries):
        raise AssertionError("Template layout changed")
    for entry in parsed:
        data = out[entry["start"]:entry["end"]]
        if entry["name"] == "app/lua/main.lua" and data != lua.ljust(entry["length"], b" "):
            raise AssertionError("Lua round trip failed")
        if entry["name"].endswith(".elf") and data != elf.ljust(entry["length"], b"\0"):
            raise AssertionError("ELF round trip failed")
    return bytes(out), parsed

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("template",type=Path); p.add_argument("lua",type=Path)
    p.add_argument("elf",type=Path); p.add_argument("output",type=Path)
    a=p.parse_args()
    data, entries=build(a.template.read_bytes(),a.lua.read_bytes(),a.elf.read_bytes())
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_bytes(data)
    evidence=dict(template_sha256=EXPECTED,sha256=hashlib.sha256(data).hexdigest(),
                  bytes=len(data),id="999999997",records=entries,
                  note="Preview/assets retain template images; all Lua and executable code replaced.")
    a.output.with_suffix(".manifest.json").write_text(json.dumps(evidence,indent=2))
    print(json.dumps({k:evidence[k] for k in ("sha256","bytes","id")}))

if __name__=="__main__": main()
