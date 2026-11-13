#!/usr/bin/env python3
"""Run isolated catalog persistence/filter tests on the explicitly selected ADB device."""
from pathlib import Path
import argparse
import importlib.util
import subprocess
import shutil

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('android_build', ROOT / 'build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--serial', required=True)
    args = parser.parse_args()
    out = ROOT / 'build/device-tests'
    if out.exists(): shutil.rmtree(out)
    (out / 'classes').mkdir(parents=True)
    (out / 'dex').mkdir()
    manifest = out / 'AndroidManifest.xml'
    manifest.write_text('''<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="dev.mbapdu.tags.tests">
    <uses-sdk android:minSdkVersion="33" android:targetSdkVersion="36" />
    <application android:label="Catalog test harness" android:debuggable="true" />
    <instrumentation android:name="dev.mbapdu.tags.CatalogDeviceTest" android:targetPackage="dev.mbapdu.tags" />
    </manifest>''')
    build.run([build.JAVA/'bin/javac', '--release', '8', '-cp', str(build.ANDROID)+':'+str(ROOT/'build/classes'), '-d', out/'classes', *sorted((ROOT/'device-tests').rglob('*.java'))])
    build.run([build.BT/'d8', '--min-api', '33', '--lib', build.ANDROID, '--classpath', ROOT/'build/classes', '--output', out/'dex', *sorted((out/'classes').rglob('*.class'))])
    build.run([build.BT/'aapt2', 'link', '-I', build.ANDROID, '--manifest', manifest, '-o', out/'unsigned.apk'])
    build.append_dex(out/'unsigned.apk',out/'dex')
    build.run([build.BT/'zipalign','-f','4',out/'unsigned.apk',out/'aligned.apk'])
    build.run([build.BT/'apksigner','sign','--ks',build.debug_signer(build.PROJECT,build.JAVA,build.ENV),'--ks-key-alias','lab','--ks-pass','pass:android','--key-pass','pass:android','--out',out/'tests.apk',out/'aligned.apk'])
    adb=[str(build.SDK/'platform-tools/adb'),'-s',args.serial]
    subprocess.run(adb+['install','-r',str(out/'tests.apk')],check=True)
    try:
        result=subprocess.run(adb+['shell','am','instrument','-w','dev.mbapdu.tags.tests/dev.mbapdu.tags.CatalogDeviceTest'],check=True,capture_output=True,text=True)
        print(result.stdout)
        (out/'result.txt').write_text(result.stdout)
        if 'CATALOG_TESTS=PASS' not in result.stdout: raise SystemExit('Catalog device tests failed')
    finally:
        subprocess.run(adb+['uninstall','dev.mbapdu.tags.tests'],check=True)

if __name__=='__main__': main()
