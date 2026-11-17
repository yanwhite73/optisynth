#!/usr/bin/env python3
"""Adopt reviewed dependency closure, retaining notices and source hashes."""
import hashlib, json, math, pathlib, re, shutil
root = pathlib.Path(__file__).resolve().parents[1]
cache = root / 'docs/references/mutable_review'
dest = root / 'third_party/mutable'
todo = ['plaits/dsp/drums/analog_bass_drum.h',
        'plaits/dsp/oscillator/variable_shape_oscillator.h',
        'plaits/dsp/physical_modelling/modal_voice.cc',
        'plaits/dsp/physical_modelling/resonator.cc',
        'stmlib/dsp/units.cc', 'stmlib/utils/random.cc']
records, seen = [], set()
while todo:
    path = todo.pop(0)
    if path in seen: continue
    seen.add(path)
    repo = 'stmlib' if path.startswith('stmlib/') else 'eurorack'
    relative = path[7:] if repo == 'stmlib' else path
    data = (cache / repo / relative).read_bytes()
    assert b'Permission is hereby granted' in data, path
    target = dest / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    records.append({'path': path, 'sha256': hashlib.sha256(data).hexdigest()})
    todo += re.findall(r'^#include "([^"]+)"', data.decode(), re.M)

source = (cache / 'eurorack/plaits/resources.cc').read_text()
notice = source[:source.index('#include')]
arrays = []
for name, count in [('lut_sine', 641), ('lut_stiffness', 65)]:
    match = re.search(r'const float ' + name + r'\[\] = \{(.*?)\};', source, re.S)
    assert match
    values = [float(v) for v in match[1].split(',') if v.strip()]
    assert len(values) == count and all(math.isfinite(v) for v in values)
    arrays.append(match[0])
    print(name, len(values), min(values), max(values))
selected = notice + '// Optisynth: only the two arrays used by this DSP subset.\n#include "plaits/resources.h"\nnamespace plaits {\n' + '\n'.join(arrays) + '\n}\n'
(dest / 'plaits/resources_selected.cc').write_text(selected)
records.append({'path': 'plaits/resources_selected.cc', 'source_sha256': hashlib.sha256(source.encode()).hexdigest(),
                'sha256': hashlib.sha256(selected.encode()).hexdigest(), 'extracted_arrays': ['lut_sine', 'lut_stiffness']})
shutil.copyfile(cache / 'stmlib/LICENSE', dest / 'LICENSE-stmlib')
shutil.copyfile(cache / 'eurorack/README.md', dest / 'README-upstream.md')
(dest / 'PROVENANCE.json').write_text(json.dumps({'revisions': json.loads((cache / 'receipt.json').read_text())['revisions'], 'files': records}, indent=2)+'\n')
print(len(records), 'reviewed source files adopted; no upstream scripts executed')
