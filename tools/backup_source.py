#!/usr/bin/env python3
"""Local source snapshot with a content manifest and verified, fresh-directory restore.

Run via agent-workload. No git, network, installers, preset corpus or build products.
Create: python3 -I tools/backup_source.py --output ../backups/optisynth
Verify: python3 -I tools/backup_source.py --verify ARCHIVE [--extract-to NEW_DIRECTORY]
"""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile
from datetime import datetime, timezone

WORKSPACE = Path(__file__).resolve().parents[2]
DIRECTORIES = (
    'optisynth/src', 'optisynth/tests', 'optisynth/tools', 'optisynth/docs',
    'optisynth/plugin/Source', 'optisynth/reference', 'optisynth/third_party/mutable',
    'optisynth/patches',
)
FILES = (
    'optisynth/README.md', 'optisynth/Makefile', 'optisynth/.gitignore', 'optisynth/AGENTS.md', 'optisynth/design-qa.md',
    'optisynth/plugin/Optisynth.jucer', 'optisynth/plugin/Tests/StressTest.jucer',
    'optisynth/plugin/Tests/StressTest.cpp', 'optisynth/plugin/Tests/PresetExport.h',
    'optisynth/app/Main.cpp', 'optisynth/app/OptisynthApp.jucer',
    'optisynth/app/Panel.h', 'optisynth/app/PatchIO.h', 'optisynth/app/RackEditor.h', 'optisynth/app/SampleLoader.h', 'optisynth/app/ModulationEditor.h',
    'optisynth/third_party/mutable/LICENSE-stmlib',
)
SUFFIXES = {'.h', '.cpp', '.cc', '.py', '.sh', '.md', '.json', '.optisynth'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def collect():
    paths = {WORKSPACE / name for name in FILES if (WORKSPACE / name).is_file()}
    for directory in DIRECTORIES:
        paths.update(p for p in (WORKSPACE / directory).rglob('*')
                     if p.is_file() and p.suffix in SUFFIXES and '__pycache__' not in p.parts
                     and not p.is_relative_to(WORKSPACE / 'optisynth/docs/references'))
    if any(p.is_symlink() for p in paths):
        raise ValueError('Source snapshot refuses symlinks')
    return sorted(paths)


def create(output):
    output.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')
    archive = output / f'optisynth-source-{stamp}.tar.gz'
    sources = {p.relative_to(WORKSPACE).as_posix(): p for p in collect()}
    content = {name: p.read_bytes() for name, p in sources.items()}
    manifest = {
        'schema': 1, 'created_utc': stamp,
        'scope': 'Optisynth source, docs, tests and own preset-format helper. '
                 'Excludes original software, presets, audio, builds, JUCE and Git metadata.',
        'files': {name: {'sha256': digest(data), 'bytes': len(data), 'mode': sources[name].stat().st_mode & 0o777}
                  for name, data in content.items()},
    }
    content['BACKUP_MANIFEST.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    with archive.open('xb') as output_file, tarfile.open(fileobj=output_file, mode='w:gz') as tar:
        for name, data in content.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            info.mode = manifest['files'].get(name, {}).get('mode', 0o644)
            tar.addfile(info, io.BytesIO(data))
    archive.with_suffix(archive.suffix + '.sha256').write_text(f'{digest(archive.read_bytes())}  {archive.name}\n')
    verify(archive, None)
    print(archive)


def verify(archive, destination):
    checksum = archive.with_suffix(archive.suffix + '.sha256')
    if not checksum.exists() or checksum.read_text().split()[0] != digest(archive.read_bytes()):
        raise ValueError('Missing or incorrect external archive checksum')
    content = {}
    with tarfile.open(archive, 'r:gz') as tar:
        for member in tar:
            path = PurePosixPath(member.name)
            if not member.isfile() or path.is_absolute() or '..' in path.parts or member.name in content:
                raise ValueError(f'Unsafe or duplicate member: {member.name}')
            if member.size > 20_000_000:
                raise ValueError('Unexpectedly large source member')
            content[member.name] = tar.extractfile(member).read()
    manifest = json.loads(content.pop('BACKUP_MANIFEST.json'))
    if set(content) != set(manifest['files']):
        raise ValueError('Archive member list differs from manifest')
    for name, data in content.items():
        expected = manifest['files'][name]
        if len(data) != expected['bytes'] or digest(data) != expected['sha256']:
            raise ValueError(f'Checksum mismatch: {name}')
    if destination:
        destination.mkdir(parents=True, exist_ok=False)
        for name, data in content.items():
            path = destination / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(manifest['files'][name]['mode'])
    print(f'Verified {len(content)} source files ({sum(map(len, content.values())):,} bytes).')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--output', type=Path)
    mode.add_argument('--verify', type=Path)
    parser.add_argument('--extract-to', type=Path)
    args = parser.parse_args()
    if args.output:
        if args.extract_to:
            parser.error('--extract-to requires --verify')
        create(args.output)
    else:
        verify(args.verify, args.extract_to)
