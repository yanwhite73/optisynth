#!/usr/bin/env python3
"""Read-only feature census of imported patches and the local original preset backup."""
from collections import Counter, defaultdict
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent / 'reverse-engineer/tools'))
import linplug_fxp as L

def main():
    counts = Counter()
    destinations, sources = Counter(), Counter()
    examples = defaultdict(list)
    patches = sorted((ROOT / 'presets-imported').glob('*.ptch'))
    for file in patches:
        fields = {}
        for line in file.read_text().splitlines():
            if line and not line.startswith('#') and ' ' in line:
                key, value = line.split(' ', 1)
                fields[key] = value
        rm = False
        for osc in range(1, 5):
            prefix = f'osc.{osc}'
            if fields.get(prefix + '.on') != '1' or fields.get(prefix + '.filter.on') != '1':
                continue
            for slot in (1, 2):
                if fields.get(f'{prefix}.filter.xmod{slot}.type') == '1' and float(fields.get(f'{prefix}.filter.xmod{slot}.amount', 0)) != 0:
                    rm = True
        if rm:
            counts['enabled_filter_rm_nonzero_base'] += 1
            if len(examples['filter_rm']) < 8:
                examples['filter_rm'].append(file.stem)
        bad_dest, bad_source = set(), set()
        for slot in range(1, 25):
            src = fields.get(f'matrix.{slot}.source', 'none')
            dst = fields.get(f'matrix.{slot}.dest', 'none')
            # Keep zero-depth rows: another matrix row can modulate their depth.
            if src == 'none' or dst == 'none':
                continue
            if dst.startswith('raw_'):
                bad_dest.add(dst)
            if src.startswith('raw_') or src == 'glide':
                bad_source.add(src)
        destinations.update(bad_dest)
        sources.update(bad_source)
        counts['presets_with_unimplemented_matrix_destination'] += bool(bad_dest)
        counts['presets_with_unimplemented_matrix_source'] += bool(bad_source)

    originals = sorted((ROOT.parent / 'reverse-engineer/corpus/backup-LinPlug/Spectral/Sound Banks').rglob('*.fxp'))
    original_counts, precision, glide_modes = Counter(), Counter(), Counter()
    errors = []
    for file in originals:
        try:
            rows = L.named_fields(L.parse(str(file)))
            values = {(tag, inst, name): value for tag, inst, name, value, _ in rows}
            exact = values.get(('SYG3', 0, 'Exactness'))
            mode = values.get(('MRG0', 0, 'Glide Mode'))
            if exact is not None:
                precision[str(exact)] += 1
                original_counts['precision_below_100'] += isinstance(exact, (int, float)) and exact < 100
            if mode is not None:
                glide_modes[str(mode)] += 1
                original_counts['glide_enabled_or_bend'] += (mode != 0 if isinstance(mode, (int, float))
                                                          else str(mode).strip().upper() != 'OFF')
            for field, name in [('Pit Scale', 'envelope_pitch_scaling_nonzero'), ('Vel Scale', 'envelope_velocity_scaling_nonzero')]:
                original_counts[name] += any(tag == 'ME05' and label == field and isinstance(value, (int, float)) and value != 0
                                             for tag, _, label, value, _ in rows)
        except Exception as error:
            errors.append({'file': file.name, 'error': str(error)})
    report = {
        'imported_preset_count': len(patches), 'imported_feature_counts': dict(counts),
        'unsupported_destinations_by_preset': destinations.most_common(),
        'unsupported_sources_by_preset': sources.most_common(), 'examples': dict(examples),
        'original_backup_count': len(originals), 'original_feature_counts': dict(original_counts),
        'precision_distribution': dict(precision), 'glide_mode_distribution': dict(glide_modes), 'errors': errors,
        'limits': 'Presence is not proof of audible impact. RM count excludes zero-base amounts driven by the matrix. '
                  'Envelope counts include unused envelopes. Original backup and imported collections differ; counts are not directly comparable.',
    }
    destination = ROOT / 'docs/legacy/optispectral/feature_usage.json'
    destination.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'imported': len(patches), 'imported_counts': dict(counts),
                      'originals': len(originals), 'original_counts': dict(original_counts),
                      'top_missing_destinations': destinations.most_common(10), 'errors': len(errors)}, indent=2))
    return bool(errors)

if __name__ == '__main__':
    raise SystemExit(main())
