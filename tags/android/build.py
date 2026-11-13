#!/usr/bin/env python3
"""Offline Android SDK build; no Gradle, external dependencies, ADB or installation."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
PROJECT = ROOT.parents[1]
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
    core = [ROOT / ('src/dev/mbapdu/tags/' + name + '.java')
            for name in ['Protocol', 'Type4Ndef', 'CommandGate', 'ReadbackProof', 'NdefReadback', 'CacheRefreshGate', 'CatalogWire', 'SelectionGate']]
    test_sources = sorted((ROOT / 'tests').rglob('*.java'))
    run([JAVA / 'bin/javac', '--release', '8', '-d', BUILD / 'tests'] + core + test_sources)
    test_output = subprocess.check_output([str(JAVA / 'bin/java'), '-cp', str(BUILD / 'tests'), 'dev.mbapdu.tags.TagTest'], text=True, env=ENV, cwd=ROOT)
    print(test_output, end='')
    assertion_match = re.search(r'^PASS (\d+) assertions:', test_output, re.M)
    if not assertion_match: raise SystemExit('Missing JVM test success marker')
    assertion_count = int(assertion_match.group(1))
    if '--test-only' in sys.argv: return
    sources = sorted((ROOT / 'src').rglob('*.java'))
    run([JAVA / 'bin/javac', '--release', '8', '-Xlint:deprecation', '-cp', ANDROID, '-d', BUILD / 'classes'] + sources)
    run([BT / 'd8', '--min-api', '33', '--lib', ANDROID, '--output', BUILD / 'dex'] + sorted((BUILD / 'classes').rglob('*.class')))
    raw = BUILD / 'unsigned.apk'
    resources = BUILD / 'resources.zip'
    run([BT / 'aapt2', 'compile', '--dir', ROOT / 'res', '-o', resources])
    run([BT / 'aapt2', 'link', '-I', ANDROID, '--manifest', ROOT / 'AndroidManifest.xml', '-o', raw,
         '--min-sdk-version', '33', '--target-sdk-version', '36', '--version-code', '5', '--version-name', '0.4.1', resources])
    append_dex(raw, BUILD / 'dex')
    aligned = BUILD / 'aligned.apk'
    run([BT / 'zipalign', '-f', '4', raw, aligned])
    key = debug_signer(PROJECT, JAVA, ENV)
    apk = BUILD / 'tag-bridge-debug.apk'
    run([BT / 'apksigner', 'sign', '--ks', key, '--ks-key-alias', 'lab', '--ks-pass', 'pass:android',
         '--key-pass', 'pass:android', '--out', apk, aligned])
    run([BT / 'apksigner', 'verify', '--verbose', apk])
    run([BT / 'zipalign', '-c', '4', apk])
    permissions = subprocess.check_output([str(BT / 'aapt'), 'dump', 'permissions', str(apk)], text=True, env=ENV)
    expected = {'android.permission.NFC', 'android.permission.BLUETOOTH_SCAN', 'android.permission.BLUETOOTH_CONNECT'}
    actual = set(re.findall(r"uses-permission: name='([^']+)'", permissions))
    if actual != expected: raise SystemExit('Unexpected APK permissions: ' + repr(actual))
    result = {'apk': str(apk), 'sha256': hashlib.sha256(apk.read_bytes()).hexdigest(), 'bytes': apk.stat().st_size,
              'min_sdk': 33, 'target_sdk': 36, 'version_code': 5, 'version_name': '0.4.1', 'permissions': sorted(actual),
              'tests': 'JVM Type4 NDEF/catalog codec/selection identity and session gate tests passed',
              'jvm_assertions': assertion_count, 'hardware_tested': False, 'installed': False,
              'source_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in sorted(sources + test_sources + list((ROOT / 'res').rglob('*.xml')) + [ROOT / 'build.py', ROOT / 'AndroidManifest.xml'])}}
    (BUILD / 'build-result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))

if __name__ == '__main__': main()
