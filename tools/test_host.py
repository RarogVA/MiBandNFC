#!/usr/bin/env python3
"""Run deterministic native and JVM tests. Does not connect to any device."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build' / 'host-tests'
OUT.mkdir(parents=True, exist_ok=True)
CC = os.environ.get('CC', 'clang')
FLAGS = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-g', '-Itests',
         '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
TESTS = {
    'route_table': ['tests/route_table_test.c', 'band/route_table.c'],
    'transport': ['tests/transport_test.c'],
    'module': ['tests/module_test.c'],
    'nfc_probe': ['-DNFC_PROBE_HOST_TEST=1', 'tests/nfc_probe_test.c',
                  'band/nfc_probe.c', 'band/route_table.c'],
    'diagnostic': ['-DDIAGNOSTIC_HOST_TEST=1', 'tests/diagnostic_test.c',
                   'band/diagnostic.c'],
}

def run(args):
    result = subprocess.run([str(a) for a in args], cwd=ROOT,
                            capture_output=True, text=True)
    if result.returncode:
        print(result.stdout + result.stderr, file=sys.stderr)
        result.check_returncode()
    return (result.stdout + result.stderr).strip()

results = {}
for name, sources in TESTS.items():
    binary = OUT / name
    run([CC, *FLAGS, *sources, '-o', binary])
    results[name] = run([binary])
    print(results[name])
results['android_jvm'] = run([sys.executable, ROOT / 'android/build.py', '--test-only'])
print(results['android_jvm'])
tracked = list((ROOT / 'band').glob('*.[ch]')) + list((ROOT / 'tests').glob('*.[ch]'))
report = {
    'status': 'PASS',
    'scope': 'Host mocks and JVM protocol/proof tests; no physical NFC/BLE claim',
    'native_flags': FLAGS,
    'tests': results,
    'sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted(tracked)},
}
(OUT / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
