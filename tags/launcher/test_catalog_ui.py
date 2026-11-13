#!/usr/bin/env python3
"""Sanitized host fault injection. No device access and no firmware execution."""
from pathlib import Path
import hashlib
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tags/launcher/build"
OUT.mkdir(parents=True, exist_ok=True)
sources = ["tags/launcher/test_catalog_ui.c", "tags/launcher/test_catalog_ui.py", "tags/launcher/catalog_ui.c",
           "tags/launcher/catalog_ui_bindings.c", "tags/launcher/catalog_ui_bindings.h",
           "tags/launcher/bindings.h", "tags/launcher/abi.h", "tags/catalog/api.h",
           "tags/bootstrap/launch.c", "tags/bootstrap/launch.h"]
firmware = (ROOT / "references/vela_ap.bin").read_bytes()
assert hashlib.sha256(firmware).hexdigest() == "66701c11073aebbd1d925cb6e097aa56a3101fc747d9f0fc280cd250dce72a40"
fingerprints = []
for source in ("tags/launcher/catalog_ui_bindings.c", "tags/bootstrap/launch.c"):
    for address, values in re.findall(r"\{\s*(0x[0-9a-fA-F]+)u?,\s*\{([^}]+)\}\}", (ROOT/source).read_text()):
        expected = bytes(int(value.strip(), 16) for value in values.split(",") if value.strip())
        offset = int(address, 16) - 0x2c0c0000
        assert len(expected) == 24 and offset >= 0
        assert firmware[offset:offset+24] == expected, (source, address)
        fingerprints.append(address)
assert len(fingerprints) == 10
# The only exposed loader command has fixed module name and path. The C mock
# proves all UI paths establish a persistent attempt guard before invoking it.
launch = (ROOT / "tags/bootstrap/launch.c").read_text()
assert re.findall(r'"insmod [^"]+"', launch) == ['"insmod /data/tag_catalog/core-catalog.elf Tag_Catalog_V1"']
exe = OUT / "host_catalog_ui_test"
subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-g",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "tags/launcher/test_catalog_ui.c", "-o", str(exe)], cwd=ROOT, check=True)
result = subprocess.run([str(exe)], cwd=ROOT, text=True, capture_output=True)
report = {"result": "PASS" if result.returncode == 0 else "FAIL",
          "ui_profile": "catalog-ui-v2",
          "sanitizers": ["address", "undefined"], "output": result.stdout,
          "errors": result.stderr, "firmware_fingerprints": fingerprints,
          "layout_scope": "336x480 containment; 256x128 card aligned with navigation/refresh; compact status and icon-only reload; no heading/module label; sibling separation and font-style choice. Actual glyph rendering requires hardware.",
          "hardware_tested": False,
          "source_sha256": {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in sources}}
(OUT / "catalog-ui-host-tests.json").write_text(json.dumps(report, indent=2) + "\n")
print(result.stdout, end="")
if result.stderr:
    print(result.stderr, end="")
raise SystemExit(result.returncode)
