#!/usr/bin/env python3
"""Restore missing zero-base depth targets without rewriting other imported settings.

Uses the local corpus only. Checks all candidates first, backs up every changed file,
and refuses ambiguous source mappings rather than choosing a same-named preset silently.
"""
from collections import defaultdict
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from import_linplug import matrix_source, matrix_dest, fmt, convert
sys.path.insert(0, str(ROOT.parent / 'reverse-engineer/tools'))
import linplug_fxp as L


def main():
    sources = defaultdict(list)
    for path in (ROOT.parent / 'reverse-engineer/corpus/backup-LinPlug/Spectral/Sound Banks').rglob('*.fxp'):
        sources[path.name].append(path)
    plans=[]
    for patch in sorted((ROOT / 'presets-imported').glob('*.ptch')):
        before=patch.read_text()
        targets=set(map(int,re.findall(r'^matrix\.\d+\.dest matrix\.depth(\d+)$',before,re.M)))
        existing=set(map(int,re.findall(r'^matrix\.(\d+)\.source ',before,re.M)))
        needed=targets-existing
        if not needed:
            continue
        match=re.search(r'^# imported from (.+\.fxp) \(LinPlug Spectral preset\)$',before,re.M)
        if not match:
            continue
        candidates=[]
        for source in sources[match[1]]:
            values={name:value for tag,inst,name,value,_ in L.named_fields(L.parse(source)) if tag=='PMX0'}
            rows=[]
            for slot in sorted(needed):
                src=matrix_source(values.get(f'Source {slot}',''))
                dst=matrix_dest(values.get(f'Destination {slot}',''))
                amount=values.get(f'Amount {slot}',0)/100
                if src!='none' and dst!='none' and amount==0:
                    rows.extend([f'matrix.{slot}.source {src}',f'matrix.{slot}.dest {dst}',
                                 f'matrix.{slot}.amount {fmt(amount)}'])
            candidates.append((source,'\n'.join(rows)))
        if not candidates:
            raise RuntimeError(f'Missing source for {patch.name}')
        if len({rows for _,rows in candidates})!=1:
            # Flattened imports can share a basename. Match their actual existing
            # parameter data before deciding which zero rows belong to this patch.
            def fields(text):
                return dict(line.split(' ',1) for line in text.splitlines()
                            if line and not line.startswith('#') and ' ' in line)
            old=fields(before)
            matching=[]
            for source,rows in candidates:
                new=fields(convert(L,str(source))[0])
                if all(new.get(k)==v for k,v in old.items()):
                    matching.append((source,rows))
            candidates=matching
            if not candidates or len({rows for _,rows in candidates})!=1:
                raise RuntimeError(f'Ambiguous zero-depth rows after matching existing parameters: {patch.name}')
        addition=candidates[0][1]
        if not addition:
            continue
        after=before.rstrip()+'\n# Restored zero-base matrix depth targets\n'+addition+'\n'
        backup=ROOT/'build/matrix_zero_backup'/patch.name
        if backup.exists() and backup.read_text()!=before:
            raise RuntimeError(f'Conflicting backup: {backup}')
        plans.append((patch,backup,before,after,addition.count('.source ')))
    for patch,backup,before,after,_ in plans:
        backup.parent.mkdir(exist_ok=True)
        if not backup.exists():
            backup.write_text(before)
        if patch.read_text()!=before:
            raise RuntimeError(f'Patch changed during planning: {patch}')
        patch.write_text(after)
        assert patch.read_text()==after
    print(f'Restored {sum(p[4] for p in plans)} zero-depth target rows in {len(plans)} patches; backups in build/matrix_zero_backup')


if __name__=='__main__':
    main()
