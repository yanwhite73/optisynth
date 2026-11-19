"""Add Optisynth's icon to an unsigned or staged macOS bundle before final signing."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess

ICON = Path(__file__).resolve().parents[1] / 'app/Artwork/Optisynth.icns'


def package_icon(bundle):
    plist = bundle / 'Contents/Info.plist'
    original = plist.read_bytes()
    info = plistlib.loads(original)
    if info.get('CFBundleIdentifier') != 'com.yanwhite.optisynth.app':
        raise ValueError(f'Unexpected bundle identity in {plist}')
    resources = bundle / 'Contents/Resources'
    resources.mkdir(exist_ok=True)
    shutil.copyfile(ICON, resources / ICON.name)
    info['CFBundleIconFile'] = ICON.name
    info.pop('CFBundleIconName', None)
    fmt = plistlib.FMT_BINARY if original.startswith(b'bplist') else plistlib.FMT_XML
    plist.write_bytes(plistlib.dumps(info, fmt=fmt, sort_keys=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--sign', action='store_true')
    args = parser.parse_args()
    package_icon(args.bundle)
    if args.sign:
        subprocess.run(['codesign', '--force', '--sign', '-', str(args.bundle)], check=True)
        subprocess.run(['codesign', '--verify', '--strict', '--deep', str(args.bundle)], check=True)
