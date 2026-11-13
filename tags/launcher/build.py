#!/usr/bin/env python3
"""Build the independent read-only NFC Tags page; never install it."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import argparse

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("tag_builder",ROOT/"tags/build_band.py")
builder=importlib.util.module_from_spec(spec);spec.loader.exec_module(builder)
OUT=ROOT/"tags/launcher/build"
FW_SHA="66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40"

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scroll-v2",action="store_true",help="Build scrollable UI v2 separately; default reproduces frozen v1")
    args=parser.parse_args();suffix="-v2" if args.scroll_v2 else ""
    firmware=(ROOT/"references/vela_ap.bin").read_bytes()
    if hashlib.sha256(firmware).hexdigest()!=FW_SHA:raise SystemExit("Wrong firmware image")
    header=(ROOT/"tags/launcher/fingerprints.h").read_text()
    if args.scroll_v2:header+=(ROOT/"tags/launcher/scroll_fingerprints.h").read_text()
    checked=[]
    for address,values in re.findall(r"\{(0x[0-9a-f]+)u,\{([^}]+)\}\}",header):
        addr=int(address,16);data=bytes(int(x,16) for x in values.split(","))
        if len(data)!=24 or firmware[addr-0x2c0c0000:addr-0x2c0c0000+24]!=data:
            raise SystemExit("Wrong fingerprint "+address)
        checked.append(address)
    bindings=(ROOT/"tags/launcher/bindings.h").read_text()
    if args.scroll_v2:bindings+=(ROOT/"tags/launcher/scroll_bindings.h").read_text()
    required=set(re.findall(r"UI_FN\((0x[0-9a-f]+)u",bindings))|{"0x2c7abb68"}
    if args.scroll_v2:required|={"0x2c7f087a","0x2c7f0a14","0x2c7f0380"}
    if required!=set(checked):raise SystemExit("Missing or unused fingerprint")
    # The ROM initialization table contains the package-manager install
    # target. The runtime check separately verifies SRAM slot 0x200b72e0.
    if firmware[0x2ca9bf74-0x2c0c0000:0x2ca9bf78-0x2c0c0000]!=bytes.fromhex("69bb7a0c"):
        raise SystemExit("Package-manager dispatch initializer changed")
    OUT.mkdir(parents=True,exist_ok=True)
    sources=["tags/launcher/module.c","tags/launcher/state.c","tags/launcher/firmware.c","band/runtime.c"]
    flags=[x for x in builder.FLAGS if not x.startswith("-DNFC_") and not x.startswith("-DTRANSPORT_")]
    if args.scroll_v2:flags+=["-DTAG_UI_SCROLL_V2=1"]
    objects=[]
    for source in sources:
        obj=OUT/(Path(source).stem+suffix+".o")
        builder.run([builder.BIN/"clang",*flags,"-I",ROOT/"tags/launcher","-c",ROOT/source,"-o",obj])
        builder.run([builder.BIN/"llvm-objcopy","--remove-section=.ARM.exidx*","--remove-section=.comment",obj])
        objects.append(obj)
    elf=OUT/("nfc_tags_ui"+suffix+".elf")
    builder.run([builder.BIN/"ld.lld","-r","-T",ROOT/"band/module.ld","-o",elf,*objects])
    before=elf.read_bytes();builder.check_elf(before)
    builder.run([builder.BIN/"llvm-objcopy","--strip-unneeded","--keep-symbol=module_constructor",elf])
    data=elf.read_bytes()
    if builder.elf_semantics(before)!=builder.elf_semantics(data):raise SystemExit("Strip changed ELF semantics")
    undefined=builder.run([builder.BIN/"llvm-nm","--undefined-only",elf]).strip()
    if undefined:raise SystemExit("Unresolved symbols: "+undefined)
    entry,constructor=builder.check_elf(data)
    if len(data)>9841:raise SystemExit(f"Launcher exceeds reserved slot: {len(data)} > 9841")
    inputs=sources+[str(p.relative_to(ROOT)) for p in (ROOT/"tags/launcher").glob("*.h")]+["band/module.ld","tags/launcher/build.py"]
    report=dict(model="miwear.watch.p67glt",firmware="3.201.026",firmware_sha256=FW_SHA,ui_version=2 if args.scroll_v2 else 1,
                app_id=212,app_name="dev.mbapdu.tags",page_id=0,elf_bytes=len(data),slot_bytes=9841,
                elf_sha256=hashlib.sha256(data).hexdigest(),constructor=constructor,
                fingerprints=checked,hardware_validated=False,
                source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sorted(inputs)})
    (OUT/("build"+suffix+".json")).write_text(json.dumps(report,indent=2)+"\n")
    (OUT/("inspection"+suffix+".txt")).write_text(builder.run([builder.BIN/"llvm-readelf","-h","-S","-r",elf]))
    print(json.dumps({k:report[k] for k in ("elf_bytes","slot_bytes","elf_sha256","hardware_validated")},indent=2))

if __name__=="__main__":main()
