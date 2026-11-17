#!/usr/bin/env python3
"""Recover Spectral programs from the hash-bound Fushigi bank inventory.

An observed PROG section is uint32 LE program index followed by one packed
Spectral program. Only this bounded, completely consumed layout is accepted.
Preserves the packed program verbatim in an FXP wrapper and proves a strict
decode/encode round trip before producing an Optispectral patch.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

BASE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(BASE / 'reverse-engineer/tools'))
import linplug_fxp as lp
from import_linplug import convert


def sha(b):
    return hashlib.sha256(b).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('inventory', type=Path)
    args = ap.parse_args()
    inventory = json.loads(args.inventory.read_text())
    base = args.inventory.parent
    out = base / 'programs'
    out.mkdir(exist_ok=True)
    corpus = BASE / 'reverse-engineer/corpus/backup-LinPlug/Spectral/Sound Banks'
    reports, seen = [], set()
    for project in inventory['projects']:
        for state in project['spectral_states']:
            if state['sha256'] in seen:
                continue
            seen.add(state['sha256'])
            bank = (base / state['saved_as']).read_bytes()
            assert sha(bank) == state['sha256']
            assert bank[:4] == b'CcnK' and bank[8:12] == b'FBCh' and bank[16:20] == b'LSP1'
            assert struct.unpack('>I', bank[156:160])[0] == len(bank) - 160
            outer = lp.split_sections(bank[160:])
            labels = set()
            for tag, body in outer:
                if tag == 'SYG1':
                    for m in re.finditer(rb'/Library/[^\x00]+\.fxp', body):
                        labels.add(m.group().decode('latin1'))
            assert len(labels) == 1, f'Expected one saved preset path, found {labels}'
            original_path = next(iter(labels))
            relative = original_path.split('/Sound Banks/', 1)[1]
            factory = corpus / relative
            assert factory.is_file(), f'No corpus header for {relative}'
            head = lp.read_fxp(str(factory))
            occurrences = collections.Counter()
            for tag, packed_body in outer:
                if tag != 'PROG':
                    continue
                index = struct.unpack('<I', packed_body[:4])[0]
                occurrences[index] += 1
                assert occurrences[index] == 1, 'Duplicate program index'
                packed = packed_body[4:]
                raw, left = lp.unpack_runs(packed)
                assert left == 0, 'Unconsumed program bytes'
                name = Path(relative).stem
                prefix = f'{state["sha256"][:12]}-{index}'
                fxp = out / (prefix + '.fxp')
                fxp.write_bytes(lp.build_fxp(head, packed, packed=False))
                parsed = lp.parse(str(fxp))
                assert all(s['strict'] for s in parsed['sections'])
                assert lp.encode_sections(parsed['sections']) == raw
                assert lp.read_fxp(str(fxp))['chunk'] == packed
                patch_text, unmapped = convert(lp, str(fxp))
                ptch = out / (prefix + '.ptch')
                ptch.write_text(patch_text)
                factory_parsed = lp.parse(str(factory))
                before = {(tag, inst, key): value for tag, inst, key, value, _ in lp.named_fields(factory_parsed)}
                after = {(tag, inst, key): value for tag, inst, key, value, _ in lp.named_fields(parsed)}
                changes = []
                for key in sorted(set(before) | set(after)):
                    if before.get(key) != after.get(key):
                        changes.append({'section': key[0], 'instance': key[1], 'field': key[2],
                                        'corpus': before.get(key), 'saved': after.get(key)})
                record = {'project': project['path'], 'project_sha256': project['sha256'],
                          'bank_sha256': state['sha256'], 'program_index': index,
                          'saved_preset_path': original_path, 'saved_preset_label': name,
                          'raw_program_sha256': sha(raw), 'packed_program_sha256': sha(packed),
                          'fxp': str(fxp.relative_to(base)), 'fxp_sha256': sha(fxp.read_bytes()),
                          'ptch': str(ptch.relative_to(base)), 'ptch_sha256': sha(ptch.read_bytes()),
                          'decoded_bytes': len(raw), 'sections': len(parsed['sections']),
                          'strict_roundtrip_exact': True, 'unmapped_fields': unmapped,
                          'corpus_path': str(factory), 'corpus_sha256': sha(factory.read_bytes()),
                          'corpus_program_exact': lp.section_bytes(head) == raw,
                          'field_changes': changes,
                          'scope': 'Program only; original outer bank/tuning retained separately. Bounce match unverified.'}
                reports.append(record)
                print(json.dumps({k: record[k] for k in ['project', 'saved_preset_label', 'corpus_program_exact', 'unmapped_fields']}))
                print('Changed fields:', [(c['section'], c['instance'], c['field']) for c in changes][:15])
    (base / 'programs.json').write_text(json.dumps(reports, indent=2) + '\n')
    print(f'{len(reports)} programs extracted, strict round trips passed')


if __name__ == '__main__':
    main()
