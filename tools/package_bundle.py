#!/usr/bin/env python3
"""Bundle the clean project and immutable releases; no keys, caches or device I/O."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PREFIX = 'apdu-band-bridge/'
selected = set()


def include(path):
    if not path.is_file() or path.is_symlink():
        raise ValueError('Expected a regular project file: ' + str(path))
    relative = path.relative_to(ROOT)
    if any(part in {'.git', '.local', 'build', '__pycache__'} for part in relative.parts):
        raise ValueError('Private/generated file cannot be bundled: ' + str(relative))
    if path.suffix in {'.keystore', '.jks', '.key', '.pyc'}:
        raise ValueError('Private/generated file cannot be bundled: ' + str(relative))
    selected.add(path)


for name in ['README.md', '.gitignore', 'Makefile', 'NOTICE.md', 'LICENSE']:
    path = ROOT / name
    if path.exists(): include(path)
if not (ROOT / 'README.md').is_file():
    raise SystemExit('Project README.md is required before packaging.')

for folder, patterns in {
    'android': ['build.py', 'AndroidManifest.xml', 'README.md', 'src/**/*.java', 'tests/**/*.java'],
    'band': ['*.c', '*.h', '*.ld'],
    'tests': ['*.c', '*.h'],
    'tools': ['*.py'],
    'watchface': ['*.lua'],
    'diagnostics/mirpay-access': ['build.py', 'AndroidManifest.xml', 'README.txt', 'README.md', 'src/**/*.java'],
    'docs': ['**/*.md', '**/*.txt', '**/*.json', '**/*.java'],
    'docs/concepts': ['*.png', '*.py'],
    'tags': ['README.md', 'build_band.py', 'build_launcher_face.py', 'test_host.py'],
    'tags/android': ['build.py', 'test_device.py', 'AndroidManifest.xml', 'README.md', 'src/**/*.java', 'tests/**/*.java', 'device-tests/**/*.java'],
    'tags/android/res': ['**/*.xml', '**/*.png'],
    'tags/band': ['*.c', '*.h'],
    'tags/catalog': ['*.c', '*.h', '*.py', '*.md', 'tests/*.c', 'tests/*.h'],
    'tags/bootstrap': ['*.c', '*.h', '*.py', '*.lua', '*.md', '*.json', '*.png'],
    'tags/launcher': ['*.c', '*.h', '*.ld', '*.py', '*.md', '*.json', 'THIRD_PARTY_LICENSE'],
    'tags/launcher/icons': ['*.py', '*.svg', '*.png', '*.bin', '*.json', 'LICENSE'],
    'tags/tests': ['*.c', '*.h'],
    'tags/watchface': ['*.lua'],
    'tags/ui-research': ['*.py', '**/*.md', '**/*.txt', '**/*.json'],
    'tags/roundabout': ['*.py', '*.md', '*.json'],
    'tags/roundabout/shortcut-assets': ['*.png', '*.json'],
}.items():
    for pattern in patterns:
        for path in (ROOT / folder).glob(pattern):
            include(path)

provenance = json.loads((ROOT / 'references/provenance.json').read_text())
include(ROOT / 'references/provenance.json')
for name in ['recorder.face', 'vela_ap.bin']:
    path = ROOT / 'references' / name
    if hashlib.sha256(path.read_bytes()).hexdigest() != provenance[name]['sha256']:
        raise ValueError('Reference checksum mismatch: ' + name)
    include(path)

roundabout_provenance = ROOT / 'references/roundabout-original.provenance.json'
if roundabout_provenance.exists():
    record = json.loads(roundabout_provenance.read_text())
    if record['archive'] != 'roundabout-original.zip':
        raise ValueError('Unexpected Roundabout input archive name')
    archive_path = ROOT / 'references' / record['archive']
    payload = archive_path.read_bytes()
    if len(payload) != record['bytes'] or hashlib.sha256(payload).hexdigest() != record['sha256']:
        raise ValueError('Roundabout input archive checksum/size mismatch')
    with zipfile.ZipFile(archive_path) as archive:
        if set(archive.namelist()) != set(record['files']):
            raise ValueError('Roundabout archive inventory mismatch')
        for name, item in record['files'].items():
            data = archive.read(name)
            if len(data) != item['bytes'] or hashlib.sha256(data).hexdigest() != item['sha256']:
                raise ValueError('Roundabout input member mismatch: ' + name)
    include(roundabout_provenance)
    include(archive_path)

release_sets = []


def include_release_set(relative_folder, optional=False):
    release_root = ROOT / relative_folder
    if optional and not release_root.exists():
        return
    if release_root.is_symlink() or not release_root.is_dir():
        raise ValueError('Expected regular release directory: ' + relative_folder)
    manifest_path = release_root / 'manifest.json'
    checksums_path = release_root / 'SHA256SUMS'
    if not manifest_path.is_file() or not checksums_path.is_file():
        raise ValueError('Release set needs both manifest.json and SHA256SUMS: ' + relative_folder)
    manifest = json.loads(manifest_path.read_text())
    files = manifest.get('files')
    # Android UI snapshots predate the multi-artifact release schema.
    if files is None and all(key in manifest for key in ('apk', 'bytes', 'sha256')):
        files = {manifest['apk']: {'bytes': manifest['bytes'], 'sha256': manifest['sha256']}}
    if not isinstance(files, dict) or not files:
        raise ValueError('Release manifest must list files: ' + relative_folder)
    checksums = []
    for name, record in files.items():
        if Path(name).name != name or not name or name in {'manifest.json', 'SHA256SUMS'}:
            raise ValueError('Release entries must be simple artifact filenames: ' + name)
        path = release_root / name
        include(path)
        payload = path.read_bytes()
        if len(payload) != record['bytes'] or hashlib.sha256(payload).hexdigest() != record['sha256']:
            raise ValueError('Release checksum/size mismatch: ' + relative_folder + '/' + name)
        checksums.append(record['sha256'] + '  ' + name)
    if checksums_path.read_text().splitlines() != checksums:
        raise ValueError('SHA256SUMS differs from manifest.json: ' + relative_folder)
    include(manifest_path)
    include(checksums_path)
    if (release_root / 'README.md').is_file():
        include(release_root / 'README.md')
    release_sets.append((relative_folder, files))


include_release_set('releases')
# New tag artifacts are included only after the maintainer explicitly freezes
# a complete, checksummed release set. Local tags/**/build outputs never enter.
include_release_set('releases/tags', optional=True)
include_release_set('releases/roundabout', optional=True)
include_release_set('releases/roundabout-nfc', optional=True)
include_release_set('releases/catalog', optional=True)
include_release_set('releases/catalog-ui4', optional=True)
include_release_set('releases/catalog-ui1', optional=True)
include_release_set('releases/tags-ui', optional=True)
include_release_set('releases/tags-ui-0.4.1', optional=True)

bundle = ROOT / 'build/apdu-band-bridge.zip'
bundle.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(bundle, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(selected):
        name = PREFIX + path.relative_to(ROOT).as_posix()
        # Stable metadata makes a bundle independent of checkout mtimes/path.
        info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
        info.create_system = 3
        info.external_attr = 0o100644 << 16
        archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED)
with zipfile.ZipFile(bundle) as archive:
    if archive.testzip() is not None:
        raise RuntimeError('Bundle integrity check failed')
    for folder, files in release_sets:
        for name, record in files.items():
            if hashlib.sha256(archive.read(PREFIX + folder + '/' + name)).hexdigest() != record['sha256']:
                raise RuntimeError('Bundled release mismatch: ' + folder + '/' + name)
print(json.dumps({'bundle': str(bundle), 'files': len(selected),
                  'bytes': bundle.stat().st_size,
                  'sha256': hashlib.sha256(bundle.read_bytes()).hexdigest()}, indent=2))
