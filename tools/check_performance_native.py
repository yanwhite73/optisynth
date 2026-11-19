#!/usr/bin/env python3
"""Run via agent-workload after building the current direct app candidate."""
from pathlib import Path
import hashlib
import json
import re
import shlex
import subprocess
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
old = root / 'app/Builds/MacOSX/build/Optisynth.build/Release/Optisynth - App.build/Objects-normal/arm64'
responses = [p for p in old.glob('*-common-args.resp') if '-std=c++17' in p.read_text() and '-fobjc-' not in p.read_text()]
if len(responses) != 1:
    raise RuntimeError('Ambiguous C++ compile configuration')
args = shlex.split(responses[0].read_text())
receipt = json.loads((root / 'build/app-candidate/build-receipt.json').read_text())
for source, expected in receipt['sources'].items():
    if hashlib.sha256((root / source).read_bytes()).hexdigest() != expected:
        raise RuntimeError('Rebuild changed source before native verification: ' + source)
project = ET.parse(root / 'app/OptisynthApp.jucer').getroot()
objects = [root / 'build/app-direct-objects' / f'{i}-{Path(item.attrib["file"]).stem}.o'
           for i, item in enumerate(project.findall('.//FILE'))
           if item.attrib.get('compile') == '1' and Path(item.attrib['file']).name != 'Main.cpp']
objects += [Path(obj['path']) for obj in receipt['juce_objects']]
compiler = '/Applications/Xcode 26.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++'
frameworks = sorted(set(re.findall(r'name = (\w+)\.framework;', (root / 'app/Builds/MacOSX/Optisynth.xcodeproj/project.pbxproj').read_text())))
command = [compiler, *args, str(root / 'tests/test_performance_native.cpp'), *map(str, objects)]
for framework in frameworks:
    command += ['-framework', framework]
binary = root / 'build/check-performance-native'
subprocess.run([*command, '-o', str(binary)], check=True, cwd=root)
subprocess.run([str(binary)], check=True, cwd=root)
subprocess.run([str(binary), '--ui'], check=True, cwd=root)
