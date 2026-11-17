#!/usr/bin/env python3
"""Render only Partials and compare with preserved, hash-checked archive references."""
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'reverse-engineer/tools'))
from probe_archive_reference import metrics, sha

OUT=ROOT/'build/resonator_comparison'
OUT.mkdir(exist_ok=True)
rows=[]
for folder,stage in [('probe_archive_reference','rich'),('probe_resonator_validation',''),
                     ('probe_archive_reference','identity')]:
    base=ROOT.parent/'reverse-engineer/renders'/folder
    original=json.loads((base/(stage+'_analysis.json' if stage else 'analysis.json')).read_text())
    for m in original:
        name=m['name']; patch=OUT/(name+'.ptch'); wav=OUT/(name+'.wav')
        assert sha(base/(name+'__archive.wav'))==m['archive']['sha256']
        args=['--raw','--sr','48000','--seconds',str(m['seconds']),
              '--note',f"{m['note']}:{m['start']}:{m['length']}:100"]
        with (OUT/(name+'.log')).open('w') as log:
            subprocess.run(['python3','-I',str(ROOT/'tools/import_linplug.py'),m['file'],str(patch)],
                check=True,stdout=log,stderr=subprocess.STDOUT)
            subprocess.run([str(ROOT/'build/partials-render'),str(patch),str(wav),*args],
                check=True,stdout=log,stderr=subprocess.STDOUT,timeout=60)
        now=metrics(wav,m.get('fundamental'))
        row=dict(name=name,group=stage or 'validation',archive=m['archive'],candidate=now,
                 previous=m.get('partials'),renderer_sha256=sha(ROOT/'build/partials-render'))
        if m.get('fundamental'):
            errors=[b['db']-a['db'] for a,b in zip(m['archive']['harmonics'],now['harmonics'])
                    if a['prominence_db']>=20 and a['db']>m['archive']['harmonics'][0]['db']-40]
            row['prominent_harmonic_rms_error_db']=(sum(e*e for e in errors)/max(1,len(errors)))**.5
        rows.append(row)
        (OUT/'analysis.json').write_text(json.dumps(rows,indent=2)+'\n')
        print(name,'early',round(now['early_rms_db']-m['archive']['early_rms_db'],2),
              'harmonic_error',round(row.get('prominent_harmonic_rms_error_db',0),2),flush=True)
