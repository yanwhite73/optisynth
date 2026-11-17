#!/usr/bin/env python3
"""Export prominent scalar harmonic measurements for our offline model experiment."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
rows = json.loads((ROOT.parent / 'reverse-engineer/renders/probe_archive_reference/rich_analysis.json').read_text())
out = ROOT / 'build/resonator_fit'
out.mkdir(exist_ok=True)
lines = []
for row in rows:
    if not row['name'].startswith('rich_osc') or not row['resonance']:
        continue
    values = [row['osc_db'], row['resonance']/100]
    for h in row['archive']['harmonics'][:12]:
        values += [h['db'], 1 if h['prominence_db'] >= 20 else 0]
    lines.append(' '.join(str(v) for v in values))
(out / 'targets.txt').write_text('\n'.join(lines) + '\n')
print(f'{len(lines)} cases exported; original audio untouched')
