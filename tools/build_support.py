"""Local build paths and the debug signer. No network or device access."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import zipfile


def android_sdk():
    configured = os.environ.get('ANDROID_SDK_ROOT') or os.environ.get('ANDROID_HOME')
    if configured:
        return Path(configured).expanduser()
    suffix = 'Library/Android/sdk' if sys.platform == 'darwin' else 'Android/Sdk'
    return Path.home() / suffix


def java_home():
    if os.environ.get('JAVA_HOME'):
        return Path(os.environ['JAVA_HOME']).expanduser()
    for candidate in ['/opt/homebrew/opt/openjdk', '/usr/local/opt/openjdk']:
        if (Path(candidate) / 'bin/javac').exists():
            return Path(candidate)
    if sys.platform == 'darwin' and Path('/usr/libexec/java_home').exists():
        result = subprocess.run(['/usr/libexec/java_home'], text=True, capture_output=True)
        if result.returncode == 0 and result.stdout.strip():
            return Path(result.stdout.strip())
    javac = shutil.which('javac')
    if javac:
        return Path(javac).resolve().parents[1]
    raise SystemExit('JDK not found. Set JAVA_HOME to a local JDK directory.')


def debug_signer(project_root, java, env):
    """Keep this key outside disposable build outputs; never include in bundles."""
    key = project_root / '.local/signing/lab-debug.keystore'
    if key.exists():
        return key
    key.parent.mkdir(parents=True, exist_ok=True)
    key.parent.parent.chmod(0o700)
    key.parent.chmod(0o700)
    subprocess.run([
        str(java / 'bin/keytool'), '-genkeypair', '-keystore', str(key),
        '-storepass', 'android', '-keypass', 'android', '-alias', 'lab',
        '-dname', 'CN=APDU Lab Local Debug', '-keyalg', 'RSA',
        '-keysize', '2048', '-validity', '3650'
    ], check=True, env=env)
    key.chmod(0o600)
    return key


def append_dex(apk_path, dex_dir):
    """Stable ZIP metadata allows repeat builds with the same toolchain and key."""
    with zipfile.ZipFile(apk_path, 'a', compression=zipfile.ZIP_DEFLATED) as apk:
        for dex in sorted(dex_dir.glob('*.dex')):
            entry = zipfile.ZipInfo(dex.name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            entry.compress_type = zipfile.ZIP_DEFLATED
            apk.writestr(entry, dex.read_bytes())
