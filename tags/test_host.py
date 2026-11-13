#!/usr/bin/env python3
"""Run native tag-profile host tests with ASAN/UBSAN; no device access.

The original APDU Lab regression suite is separate: tools/test_host.py.
Android tests are run by tags/android/build.py --test-only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tags/build/host-tests"
CC = os.environ.get("CC", "clang")
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-g",
         "-Itests", "-Iband", "-Itags/band", "-Itags/tests",
         "-DTRANSPORT_TAG_PROFILE=1",
         "-fsanitize=address,undefined",
         "-fno-omit-frame-pointer"]
TESTS = {
    "tag_record": ["tags/tests/tag_record_test.c", "tags/band/tag_record.c"],
    "tag_module": ["tags/tests/tag_module_test.c", "tags/band/tag_record.c"],
    "tag_route": ["tags/tests/tag_route_test.c", "band/route_table.c"],
    "tag_transport": ["tags/tests/tag_transport_test.c"],
    "tag_display": ["tags/tests/tag_display_test.c", "tags/band/tag_display.c"],
    "tag_nfc_probe": ["tags/tests/tag_nfc_probe_test.c", "band/nfc_probe.c", "band/route_table.c"],
}
EXTRA_FLAGS = {
    "tag_display": ["-DDIAGNOSTIC_HOST_TEST=1"],
    "tag_nfc_probe": ["-DNFC_PROBE_HOST_TEST=1"],
}


def run(args):
    result = subprocess.run([str(arg) for arg in args], cwd=ROOT, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout + result.stderr, file=sys.stderr)
        result.check_returncode()
    return (result.stdout + result.stderr).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--diagnostic-v3", action="store_true")
    args = parser.parse_args()
    out = ROOT / "tags/build/host-tests-v3" if args.diagnostic_v3 else OUT
    flags = FLAGS + (["-DTAG_DIAGNOSTIC_V3=1",
                      '-DDIAGNOSTIC_STATE_PATH="/tmp/tag_catalog_diag"'] if args.diagnostic_v3 else [])
    out.mkdir(parents=True, exist_ok=True)
    results = {}
    for name, sources in TESTS.items():
        for source in sources:
            if not (ROOT / source).is_file():
                raise SystemExit("Missing test source: " + source)
        binary = out / name
        run([CC, *flags, *EXTRA_FLAGS.get(name, []), *sources, "-o", binary])
        results[name] = run([binary])
        print(results[name])
    tracked = [path for folder in ("tags/band", "tags/tests", "band")
               for path in (ROOT / folder).glob("*.[ch]")]
    report = {
        "status": "PASS",
        "scope": "Tag-profile host mocks, metadata/APDU boundaries and route tests; no hardware claim",
        "variant": "diagnostic-v3" if args.diagnostic_v3 else "v2",
        "native_flags": flags,
        "per_suite_flags": EXTRA_FLAGS,
        "tests": results,
        "source_sha256": {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in sorted(tracked)},
    }
    (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
