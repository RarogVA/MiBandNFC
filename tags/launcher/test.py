#!/usr/bin/env python3
from pathlib import Path
import argparse,json,subprocess
ROOT=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument("--scroll-v2",action="store_true");args=parser.parse_args()
suffix="-v2" if args.scroll_v2 else ""
out=ROOT/"tags/launcher/build";out.mkdir(parents=True,exist_ok=True)
exe=out/("host_test"+suffix)
subprocess.run(["clang","-std=c11","-Wall","-Wextra","-Werror","-g","-fsanitize=address,undefined",
                "-fno-omit-frame-pointer",*(["-DTAG_UI_SCROLL_V2=1"] if args.scroll_v2 else []),
                "tags/launcher/test.c","tags/launcher/state.c","-o",str(exe)],cwd=ROOT,check=True)
result=subprocess.run([str(exe)],cwd=ROOT,check=True,text=True,capture_output=True)
(out/("host-tests"+suffix+".json")).write_text(json.dumps({"sanitizers":["address","undefined"],"ui_version":2 if args.scroll_v2 else 1,"result":"PASS","output":result.stdout},indent=2)+"\n")
print(result.stdout,end="")
