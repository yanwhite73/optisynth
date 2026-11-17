#!/usr/bin/env python3
"""Evaluate frozen model parameters on held-out scalar reference data."""
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
rows=json.loads((ROOT.parent/'reverse-engineer/renders/probe_resonator_validation/analysis.json').read_text())
out=ROOT/'build/resonator_fit'
for kind in ('saw_h2','saw_h8','sine'):
    group=[r for r in rows if r['kind']==kind]
    lines=[]
    for row in group:
        values=[row['osc_db'],row['resonance']/100]
        for h in row['archive']['harmonics'][:12]:
            values += [h['db'],1 if h['prominence_db']>=20 else 0]
        lines.append(' '.join(str(v) for v in values))
    targets=out/(kind+'_targets.txt')
    targets.write_text('\n'.join(lines)+'\n')
    result=subprocess.run([str(out/'fit'),str(targets),str(group[0]['cutoff']),
        str(group[0]['fundamental']),'1' if kind=='sine' else '16'],
        check=True,text=True,capture_output=True)
    (out/(kind+'_validation.log')).write_text(result.stdout)
    print(kind,result.stdout.splitlines()[-1])
