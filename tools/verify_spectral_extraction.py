#!/usr/bin/env python3
"""Compare the extracted Spectral reader with the original first-party renderer.

Run via agent-workload --kind test. Both executables are our offline CLI, never
the original LinPlug plugin. Uses existing private .ptch fixtures read-only.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline', type=Path, required=True)
    ap.add_argument('--presets', type=Path, required=True)
    args = ap.parse_args()
    out = ROOT/'build/spectral_extraction'
    out.mkdir(parents=True, exist_ok=True)
    candidate = ROOT/'build/partials-render'
    report = {'baseline_renderer_sha256': sha(args.baseline),
              'candidate_renderer_sha256': sha(candidate), 'fixtures': []}
    options = ['--raw', '--sr', '48000', '--seconds', '2', '--bpm', '110',
               '--ppq-start', '448', '--note', '48:0.013:0.8:96', '--note', '55:0.247:0.6:73']
    for name in ['ib Filth Bass', 'fn HiHat', 'hr Tortured Lead 2']:
        patch = args.presets/(name+'.ptch')
        outputs = []
        for label, executable in [('before', args.baseline), ('after', candidate)]:
            output = out/(name+'-'+label+'.wav')
            with (out/(name+'-'+label+'.log')).open('w') as log:
                subprocess.run([str(executable.resolve()), str(patch.resolve()), str(output), *options],
                               check=True, stdout=log, stderr=subprocess.STDOUT, timeout=90)
            outputs.append(output)
        equal = outputs[0].read_bytes() == outputs[1].read_bytes()
        row = {'patch': str(patch.resolve()), 'patch_sha256': sha(patch),
               'before_sha256': sha(outputs[0]), 'after_sha256': sha(outputs[1]),
               'byte_exact': equal}
        report['fixtures'].append(row)
        (out/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
        print(name, 'byte-exact' if equal else 'DIFFERS', flush=True)
        if not equal:
            raise RuntimeError('Spectral extraction changed compatibility audio')
    # The source-copy manifest also binds every original source file.
    snapshot = json.loads((ROOT/'docs/verification/source_snapshot_20261117.json').read_text())
    for row in snapshot['files'].values():
        if sha(ROOT.parent/row['source']) != row['sha256']:
            raise RuntimeError('Original source changed: '+row['source'])
    report['original_source_files_unchanged'] = len(snapshot['files'])
    report['render_options'] = options
    (out/'verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Original source unchanged; all three before/after renders identical.')


if __name__ == '__main__':
    main()
