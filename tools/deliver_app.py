#!/usr/bin/env python3
"""Copy a signed app to the ONE established delivery path; preserve the prior app.

File delivery only: no compilation, app launch, audio, installation or network.
Build/test callers run within their existing agent-workload reservation.
"""
import argparse
import ctypes
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
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


def swap_contents(staged, installed):
    """Atomically exchange Contents on macOS while retaining the installed folder ID."""
    swap = ctypes.CDLL(None, use_errno=True).renamex_np
    swap.argtypes = (ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint)
    swap.restype = ctypes.c_int
    rename_swap = 2  # Darwin stdio.h RENAME_SWAP
    if swap(os.fsencode(staged / 'Contents'), os.fsencode(installed / 'Contents'), rename_swap):
        err = ctypes.get_errno()
        raise OSError(err, os.strerror(err))


def copy_delivery(source, note):
    # The synth executable is self-contained. Exchange complete signed trees
    # so macOS keeps the old executable mapping alive until the user quits.
    source = source.resolve(strict=True)
    if source == APP.resolve():
        raise ValueError('Build in staging; do not overwrite the delivered app in place')
    details = inspect(source)
    RELEASE.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    # Finish the staged copy and verification before publishing it at the launch path.
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
            shutil.copytree(APP, archive / APP.name, symlinks=True)
            if inspect(archive / APP.name) != previous:
                raise ValueError('Previous app backup failed verification')
            swap_contents(staged, APP)
        else:
            staged.rename(APP)
        try:
            if inspect(APP) != details:
                raise ValueError('Installed bundle failed verification')
            APP.touch()
            staged_receipt.replace(RECEIPT)
        except BaseException:
            if archive:
                swap_contents(staged, APP)
            else:
                APP.rename(staged)
            raise
    print(json.dumps(record, indent=2))


def deliver(source, note):
    RELEASE.mkdir(parents=True, exist_ok=True)
    with (RELEASE / '.delivery.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        copy_delivery(source, note)
        # A copied bundle can retain the old directory timestamp and cached icon.
        # Advertise only the canonical app, after the signed update is installed.
        registrar = '/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'
        result = subprocess.run([registrar, '-f', str(APP)], check=False)
        if result.returncode:
            print(f'App delivered. Launch Services registration returned {result.returncode}.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--qualification', required=True,
                        help='Actual test status; copying/signing is not UI qualification')
    args = parser.parse_args()
    deliver(args.source, args.qualification)
