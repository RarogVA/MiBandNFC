#!/usr/bin/env python3
"""Offline Android SDK build; no Gradle, external dependencies, ADB or installation."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
PROJECT = ROOT.parent
sys.path.insert(0, str(PROJECT / 'tools'))
from build_support import android_sdk, java_home, debug_signer, append_dex

SDK = android_sdk()
JAVA = java_home()
BT = SDK / 'build-tools/36.0.0'
ANDROID = SDK / 'platforms/android-36/android.jar'
BUILD = ROOT / 'build'
ENV = dict(os.environ, JAVA_HOME=str(JAVA), PATH=str(JAVA / 'bin') + os.pathsep + os.environ.get('PATH', ''))

def run(args):
    subprocess.run([str(x) for x in args], check=True, env=ENV, cwd=ROOT)

def main():
    for required in [JAVA / 'bin/javac', JAVA / 'bin/keytool', ANDROID, BT / 'aapt2', BT / 'd8', BT / 'apksigner']:
        if not required.exists():
            raise SystemExit('Missing local build tool: ' + str(required))
    BUILD.mkdir(exist_ok=True)
    for name in ['classes', 'tests', 'dex']:
        p = BUILD / name
        if p.exists(): shutil.rmtree(p)
        p.mkdir()
    core = [ROOT / ('src/dev/mbapdu/probe/' + name + '.java')
            for name in ['Protocol', 'ProofJournal', 'ControlPreflight', 'InfoRequest']]
    test_sources = sorted((ROOT / 'tests').rglob('*.java'))
    run([JAVA / 'bin/javac', '--release', '8', '-d', BUILD / 'tests'] + core + test_sources)
    run([JAVA / 'bin/java', '-cp', BUILD / 'tests', 'dev.mbapdu.probe.ProtocolTest'])
    if '--test-only' in sys.argv: return
    sources = sorted((ROOT / 'src').rglob('*.java'))
    run([JAVA / 'bin/javac', '--release', '8', '-Xlint:deprecation', '-cp', ANDROID, '-d', BUILD / 'classes'] + sources)
    run([BT / 'd8', '--min-api', '33', '--lib', ANDROID, '--output', BUILD / 'dex'] + sorted((BUILD / 'classes').rglob('*.class')))
    raw = BUILD / 'unsigned.apk'
    run([BT / 'aapt2', 'link', '-I', ANDROID, '--manifest', ROOT / 'AndroidManifest.xml', '-o', raw,
         '--min-sdk-version', '33', '--target-sdk-version', '36', '--version-code', '4', '--version-name', '0.4-read-only-info'])
    append_dex(raw, BUILD / 'dex')
    aligned = BUILD / 'aligned.apk'
    run([BT / 'zipalign', '-f', '4', raw, aligned])
    key = debug_signer(PROJECT, JAVA, ENV)
    apk = BUILD / 'mbapdu-lab-debug.apk'
    run([BT / 'apksigner', 'sign', '--ks', key, '--ks-key-alias', 'lab', '--ks-pass', 'pass:android',
         '--key-pass', 'pass:android', '--out', apk, aligned])
    run([BT / 'apksigner', 'verify', '--verbose', apk])
    run([BT / 'zipalign', '-c', '4', apk])
    permissions = subprocess.check_output([str(BT / 'aapt'), 'dump', 'permissions', str(apk)], text=True, env=ENV)
    expected = {'android.permission.NFC', 'android.permission.BLUETOOTH_SCAN', 'android.permission.BLUETOOTH_CONNECT'}
    import re
    actual = set(re.findall(r"uses-permission: name='([^']+)'", permissions))
    if actual != expected: raise SystemExit('Unexpected APK permissions: ' + repr(actual))
    result = {'apk': str(apk), 'sha256': hashlib.sha256(apk.read_bytes()).hexdigest(), 'bytes': apk.stat().st_size,
              'min_sdk': 33, 'target_sdk': 36, 'version_code': 4, 'version_name': '0.4-read-only-info', 'permissions': sorted(actual),
              'tests': 'JVM protocol/proof tests passed', 'hardware_tested': False, 'installed': False}
    (BUILD / 'build-result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__': main()
