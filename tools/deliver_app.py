#!/usr/bin/env python3
"""Copy a signed app to the ONE established delivery path; preserve the prior app.

File delivery only: no compilation, app launch, audio, installation or network.
Build/test callers run within their existing agent-workload reservation.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RELEASE = ROOT / 'app/Builds/MacOSX/build/Release'
APP = RELEASE / 'Optisynth.app'
RECEIPT = RELEASE / 'Optisynth-build.json'
ARCHIVES = ROOT / 'build/app-archive'


def inspect(bundle):
    metadata = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
    if metadata.get('CFBundleExecutable') != 'Optisynth':
        raise ValueError('Not an Optisynth app bundle')
    binary = bundle / 'Contents/MacOS/Optisynth'
    if not binary.is_file() or not (binary.stat().st_mode & 0o111):
        raise ValueError('Missing or non-executable Optisynth binary')
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
    return {
        'version': metadata['CFBundleShortVersionString'],
        'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
    }


def copy_delivery(source, note):
    source = source.resolve(strict=True)
    if source == APP.resolve():
        raise ValueError('Build in staging; do not overwrite the delivered app in place')
    details = inspect(source)
    RELEASE.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    # Copy and verify BEFORE moving the previous app. A failed copy leaves it intact.
    with tempfile.TemporaryDirectory(prefix='.app-delivery-', dir=RELEASE) as temporary:
        staged = Path(temporary) / 'Optisynth.app'
        shutil.copytree(source, staged, symlinks=True)
        if inspect(staged) != details:
            raise ValueError('Copied app does not match the build')
        record = dict(details, delivered_utc=stamp, app=str(APP), built_at=str(source),
                      qualification=note, previous_app=None)
        archive = None
        if APP.exists():
            previous = inspect(APP)
            archive = ARCHIVES / (stamp + '-' + previous['version'])
            archive.mkdir(parents=True, exist_ok=False)
            record['previous_app'] = str(archive / 'Optisynth.app')
        staged_receipt = Path(temporary) / RECEIPT.name
        staged_receipt.write_text(json.dumps(record, indent=2) + '\n')
        if archive:
            if RECEIPT.exists():
                shutil.copy2(RECEIPT, archive / RECEIPT.name)
            APP.rename(archive / APP.name)
        try:
            staged.rename(APP)
            staged_receipt.replace(RECEIPT)
        except BaseException:
            if not staged.exists() and APP.exists():
                APP.rename(staged)
            if archive:
                (archive / APP.name).rename(APP)
            raise
    print(json.dumps(record, indent=2))


def deliver(source, note):
    RELEASE.mkdir(parents=True, exist_ok=True)
    with (RELEASE / '.delivery.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        copy_delivery(source, note)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--qualification', required=True,
                        help='Actual test status; copying/signing is not UI qualification')
    args = parser.parse_args()
    deliver(args.source, args.qualification)
