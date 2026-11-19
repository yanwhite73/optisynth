#!/usr/bin/env python3
"""Fallback for unavailable Projucer/Xcode desktop services. Run via agent-workload.

Rebuilds every project source with the last Xcode compile configuration and reuses
only previously built JUCE module objects. Records hashes; does not imply UI QA.
Uses the same staging/delivery paths as build_app.sh. No dependencies downloaded.
"""
from package_app_icon import package_icon
import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import re
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--no-deliver', action='store_true')
options = parser.parse_args()
subprocess.run([sys.executable, str(root / 'tools/make_app_project.py')], check=True, cwd=root)
project = ET.parse(root / 'app/OptisynthApp.jucer').getroot()
version = project.attrib['version']
objects_dir = root / 'app/Builds/MacOSX/build/Optisynth.build/Release/Optisynth - App.build/Objects-normal/arm64'
responses = [p for p in sorted(objects_dir.glob('*-common-args.resp'))
             if '-std=c++17' in p.read_text() and '-fobjc-' not in p.read_text()]
if len(responses) != 1:
    raise RuntimeError('Expected one known Xcode compile configuration; inspect before reuse')
args = shlex.split(responses[0].read_text())
hex_version = '0x' + format(sum(int(n) << (8 * (2 - i)) for i, n in enumerate(version.split('.'))), 'x')
args = [('-DJUCE_APP_VERSION=' + version if a.startswith('-DJUCE_APP_VERSION=') else
         '-DJUCE_APP_VERSION_HEX=' + hex_version if a.startswith('-DJUCE_APP_VERSION_HEX=') else a) for a in args]
compiler = '/Applications/Xcode 26.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'
out = root / 'build/app-candidate'
compiled = root / 'build/app-direct-objects'
out.mkdir(parents=True, exist_ok=True)
compiled.mkdir(parents=True, exist_ok=True)
objects, source_hashes, reused = [], {}, []
for index, item in enumerate(project.findall('.//FILE')):
    if item.attrib.get('compile') != '1':
        continue
    source = (root / 'app' / item.attrib['file']).resolve()
    target = compiled / f'{index}-{source.stem}.o'
    print('Compile', source.relative_to(root), flush=True)
    subprocess.run([compiler, *args, '-c', str(source), '-o', str(target)], check=True, cwd=root)
    objects.append(target)
    source_hashes[str(source.relative_to(root))] = hashlib.sha256(source.read_bytes()).hexdigest()
for folder in ('app', 'src'):
    for header in (root / folder).rglob('*.h'):
        if 'Builds' not in header.parts and 'JuceLibraryCode' not in header.parts:
            source_hashes[str(header.relative_to(root))] = hashlib.sha256(header.read_bytes()).hexdigest()
for line in (objects_dir / 'Optisynth.LinkFileList').read_text().splitlines():
    obj = Path(line)
    if obj.name.startswith('include_juce_'):
        objects.append(obj)
        reused.append({'path': str(obj), 'sha256': hashlib.sha256(obj.read_bytes()).hexdigest()})
if not reused:
    raise RuntimeError('No existing JUCE objects; use the normal build')
native_project = (root / 'app/Builds/MacOSX/Optisynth.xcodeproj/project.pbxproj').read_text()
frameworks = sorted(set(re.findall(r'name = (\w+)\.framework;', native_project)))
bundle = out / 'Optisynth.app'
if not bundle.exists():
    shutil.copytree(root / 'app/Builds/MacOSX/build/Release/Optisynth.app', bundle)
info = bundle / 'Contents/Info.plist'
metadata = plistlib.loads(info.read_bytes())
metadata['CFBundleShortVersionString'] = metadata['CFBundleVersion'] = version
info.write_bytes(plistlib.dumps(metadata))
binary = bundle / 'Contents/MacOS/Optisynth'
command = [compiler, '-target', 'arm64-apple-macos11', '-isysroot', args[args.index('-isysroot') + 1],
           *map(str, objects), '-Wl,-dead_strip', '-Wl,-adhoc_codesign']
for framework in frameworks:
    command += ['-framework', framework]
subprocess.run([*command, '-o', str(binary)], check=True, cwd=root)
package_icon(bundle)
subprocess.run(['codesign', '--force', '--sign', '-', str(bundle)], check=True)
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
receipt = {'version': version, 'method': 'Direct Clang; all project sources rebuilt; unchanged JUCE objects reused',
           'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
           'sources': source_hashes, 'juce_objects': reused, 'ui_verified': False}
(out / 'build-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
if not options.no_deliver:
    subprocess.run([sys.executable, str(root / 'tools/deliver_app.py'), str(bundle),
                    '--qualification', 'Direct compiler preview; native UI checks and visual review pending.'], check=True)
print('Built', bundle)
