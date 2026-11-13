#!/usr/bin/env python3
"""Build the exact-firmware experimental module; never install or execute it."""
from pathlib import Path
import hashlib
import json
import os
import struct
import subprocess
import sys
from build_support import android_sdk

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build'; OUT.mkdir(exist_ok=True)
NDK=Path(os.environ.get('APDU_NDK') or os.environ.get('ANDROID_NDK_HOME') or
         str(android_sdk()/'ndk/28.2.13676358')).expanduser()
host={'darwin':'darwin-x86_64','linux':'linux-x86_64','win32':'windows-x86_64'}.get(sys.platform)
if not host: raise SystemExit('Unsupported NDK host: '+sys.platform)
BIN=NDK/'toolchains/llvm/prebuilt'/host/'bin'
if not (BIN/'clang').exists() and not (BIN/'clang.exe').exists():
    raise SystemExit('NDK clang not found. Set APDU_NDK to a local NDK directory: '+str(NDK))
sources=['module','transport','native_clock','nfc_probe','route_table','runtime','diagnostic']
objects=[]
flags=['--target=arm-none-eabi','-mcpu=cortex-m33','-mthumb','-mfloat-abi=hard',
       '-mfpu=fpv5-sp-d16','-mno-movt','-Os','-g0','-std=c11','-Wall','-Wextra',
       '-Werror','-ffreestanding','-fno-builtin','-fno-stack-protector',
       '-fno-unwind-tables','-fno-asynchronous-unwind-tables',
       '-ffunction-sections','-fdata-sections',
       '-DNFC_PROBE_ENABLE_EXPERIMENTAL_ROUTING=1']
def run(args):
    result=subprocess.run([str(a) for a in args],capture_output=True,text=True)
    if result.returncode:
        print(result.stdout+result.stderr,file=sys.stderr)
        result.check_returncode()
    return result.stdout

for name in sources:
    obj=OUT/(name+'.o')
    run([BIN/'clang',*flags,'-I',ROOT/'band','-c',ROOT/'band'/(name+'.c'),'-o',obj])
    # C code has no exception/unwind handling. Omit linker-generated exidx
    # metadata to keep the module's relocation vocabulary minimal. The
    # non-allocated .comment compiler banner is already in band-build.json;
    # it need not consume the fixed watchface ELF resource slot.
    run([BIN/'llvm-objcopy','--remove-section=.ARM.exidx*',
         '--remove-section=.comment',obj])
    objects.append(obj)
elf=OUT/'apdu_lab_3.201.026.elf'
run([BIN/'ld.lld','-r','-T',ROOT/'band/module.ld','-o',elf,*objects])
undefined=run([BIN/'llvm-nm','--undefined-only',elf]).strip()
if undefined:raise RuntimeError('Unresolved symbols: '+undefined)
data=elf.read_bytes()
if data[:7]!=b'\x7fELF\x01\x01\x01' or struct.unpack_from('<HH',data,16)!=(1,40):
    raise RuntimeError('Unexpected ELF ABI')
entry=struct.unpack_from('<I',data,24)[0]
if not entry or not(entry&1):raise RuntimeError('Missing Thumb entry')
# Exact firmware insmod executes SHT_INIT_ARRAY, never e_entry. Inspect the
# linked artifact, including its relocation, rather than only source attributes.
shoff=struct.unpack_from('<I',data,32)[0]
shsize,shcount=struct.unpack_from('<HH',data,46)
if shsize!=40:raise RuntimeError('Unexpected section ABI')
sections=[struct.unpack_from('<10I',data,shoff+i*shsize) for i in range(shcount)]
ctors=[(i,s) for i,s in enumerate(sections) if s[1]==14]
if len(ctors)!=1 or ctors[0][1][5]!=4 or not(ctors[0][1][2]&2):
    raise RuntimeError('Need exactly one allocated 4-byte constructor array')
ctor_index,ctor_section=ctors[0]
rels=[s for s in sections if s[1]==9 and s[7]==ctor_index]
if len(rels)!=1 or rels[0][5]!=8:raise RuntimeError('Need one constructor relocation')
rel=rels[0]; reloc_offset,info=struct.unpack_from('<II',data,rel[4])
if reloc_offset!=0 or (info&255) not in (2,38):
    raise RuntimeError('Unexpected constructor relocation')
symsec=sections[rel[6]]; strs=sections[symsec[6]]
def symbol(index):
    n,v,z,kind,other,section=struct.unpack_from('<IIIBBH',data,symsec[4]+index*16)
    start=strs[4]+n; name=data[start:data.index(b'\0',start)].decode()
    return name,v,section
symbols=[symbol(i) for i in range(symsec[5]//16)]
wrapper=[s for s in symbols if s[0]=='module_constructor']
if len(wrapper)!=1 or not(wrapper[0][1]&1):raise RuntimeError('Missing Thumb constructor')
target=symbol(info>>8); addend=struct.unpack_from('<I',data,ctor_section[4])[0]
pointer=(target[1]+addend)&0xffffffff
if target[2]!=wrapper[0][2] or pointer!=wrapper[0][1]:
    raise RuntimeError('Constructor relocation does not target module_constructor')
report=run([BIN/'llvm-readelf','-h','-A','-r','-S',elf])
(OUT/'band-elf-inspection.txt').write_text(report)
manifest=dict(compiler=run([BIN/'clang','--version']).splitlines()[0],flags=flags,
              sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),entry=hex(entry),
              undefined_symbols=[],experiment_enabled=True,
              constructor=dict(section_type='SHT_INIT_ARRAY',bytes=4,
                               relocation_type=info&255,target=wrapper[0][0],
                               pointer=hex(pointer)),
              hardware_tested=False)
(OUT/'band-build.json').write_text(json.dumps(manifest,indent=2))
run([sys.executable,ROOT/'tools/package_face.py',
     ROOT/'references/recorder.face',ROOT/'watchface/main.lua',
     elf,OUT/'apdu-lab-p67glt-3.201.026.face'])
print(json.dumps(manifest,indent=2))
