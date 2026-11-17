#!/usr/bin/env python3
"""Download pinned DSP sources for review; never execute upstream code."""
import hashlib, json, pathlib, re, urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1] / 'docs/references/mutable_review'
REVS = {'eurorack': 'f4cfa7697095a28d292bff758beb51deb49b5a86',
        'stmlib': 'e3bd7c9cc00e4364166f9905c0509b6ffd0535ec'}
queue = [('eurorack', p) for p in ['README.md',
    'plaits/dsp/oscillator/variable_shape_oscillator.h',
    'plaits/dsp/drums/analog_bass_drum.h',
    'plaits/dsp/physical_modelling/modal_voice.h',
    'plaits/dsp/physical_modelling/modal_voice.cc',
    'plaits/dsp/physical_modelling/resonator.cc',
    'plaits/resources.cc', 'plaits/dsp/engine/bass_drum_engine.cc']]
queue += [('stmlib', 'README.md'), ('stmlib', 'LICENSE'), ('stmlib', 'utils/random.cc'),
          ('stmlib', 'dsp/units.cc')]
seen, receipts = set(), []
while queue:
    repo, path = queue.pop(0)
    if (repo, path) in seen:
        continue
    seen.add((repo, path))
    if '..' in pathlib.PurePosixPath(path).parts:
        raise ValueError(path)
    url = f'https://raw.githubusercontent.com/pichenettes/{repo}/{REVS[repo]}/{path}'
    dest = ROOT / repo / path
    if not dest.exists():
        with urllib.request.urlopen(url, timeout=30) as r:
            data = r.read(8_000_001)
        if len(data) > 8_000_000:
            raise ValueError('source too large')
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
    data = dest.read_bytes()
    receipts.append({'repo': repo, 'path': path, 'url': url,
                     'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    for inc in re.findall(r'^#include "([^"]+)"', data.decode(), re.M):
        queue.append(('stmlib', inc[7:]) if inc.startswith('stmlib/') else (repo, inc))
ROOT.joinpath('receipt.json').write_text(json.dumps({'revisions': REVS, 'files': receipts}, indent=2)+'\n')
print(f'{len(receipts)} pinned files downloaded for review to {ROOT}')
for repo, revision in REVS.items():
    with urllib.request.urlopen(f'https://api.github.com/repos/pichenettes/{repo}/git/trees/{revision}?recursive=1', timeout=30) as r:
        tree = json.load(r)
    paths = [i['path'] for i in tree['tree'] if i['type'] == 'blob' and
             re.search(r'(readme|changelog|history|license|release|\.md$)', i['path'], re.I)]
    ROOT.joinpath(repo + '-documentation-index.json').write_text(json.dumps(paths, indent=2)+'\n')
    print(repo, paths)
with urllib.request.urlopen('https://pichenettes.github.io/mutable-instruments-documentation/modules/plaits/images/manual.png', timeout=30) as r:
    ROOT.joinpath('plaits-panel.png').write_bytes(r.read())
