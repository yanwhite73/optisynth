#!/usr/bin/env python3
"""Repair only the formerly unmapped Env-Depth destinations in generated local patches.

All other patch text is preserved. Each changed file is backed up outside the browser's
preset directory before replacement. Run through the managed test/qualification workload.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
pattern = re.compile(r'^(matrix\.(?:[1-9]|1[0-9]|2[0-4])\.dest )raw_filter_([1-4])_env_depth$', re.MULTILINE)
changed = slots = 0
for path in sorted((ROOT / 'presets-imported').glob('*.ptch')):
    before = path.read_bytes()
    after, count = pattern.subn(r'\1filter\2.envdepth', before.decode('utf-8'))
    if not count:
        continue
    backup = ROOT / 'build/env_depth_backup' / path.name
    backup.parent.mkdir(exist_ok=True)
    if backup.exists() and backup.read_bytes() != before:
        raise RuntimeError(f'Conflicting backup: {backup}')
    backup.write_bytes(before)
    path.write_bytes(after.encode('utf-8'))
    assert path.read_text() == after
    changed += 1
    slots += count
print(f'Repaired {slots} Env-Depth destinations in {changed} patches; originals in build/env_depth_backup')
