#!/usr/bin/env python3
"""Read-only intake of Yan's Fushigi Spectral reference candidates.

Hashes audio without loading it into memory. Saves a JSON inventory and exact,
deduplicated LSP1 bank members from the projects' embedded ZIP archives. Does not
interpret Bitwig's song graph or claim that co-located audio matches a bank.
FBCh layout: JUCE juce_VSTPluginFormat.cpp, fxChunkSet (28+128+4 byte header).
Spectral payload decoding uses our existing observation-based preset reader.
"""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'reverse-engineer/tools'))
import linplug_fxp


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def wav_info(path):
    result = {'chunks': []}
    size = path.stat().st_size
    with path.open('rb') as f:
        header = f.read(12)
        if header[:4] != b'RIFF' or header[8:] != b'WAVE':
            raise ValueError('Expected RIFF/WAVE')
        end = struct.unpack('<I', header[4:8])[0] + 8
        if end != size:
            raise ValueError(f'RIFF length {end} != file length {size}')
        data_bytes = 0
        while f.tell() + 8 <= end:
            tag, length = struct.unpack('<4sI', f.read(8))
            start = f.tell()
            if start + length > end:
                raise ValueError('Chunk exceeds RIFF bounds')
            result['chunks'].append(tag.decode('ascii', errors='replace'))
            if tag == b'fmt ':
                if length < 16:
                    raise ValueError('Short format chunk')
                fmt, channels, sr, byte_rate, align, bits = struct.unpack('<HHIIHH', f.read(16))
                result.update(format_tag=fmt, channels=channels, sample_rate=sr,
                              bits_per_sample=bits, block_align=align)
                if not align or byte_rate != sr * align:
                    raise ValueError('Inconsistent format rate')
            elif tag == b'data':
                data_bytes += length
            f.seek(start + length + (length & 1))
    if 'block_align' not in result or not data_bytes:
        raise ValueError('Missing format or audio data')
    if data_bytes % result['block_align']:
        raise ValueError('Incomplete sample frame')
    result['frames'] = data_bytes // result['block_align']
    result['duration_seconds'] = result['frames'] / result['sample_rate']
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=Path, default=Path('/Users/yanwhite/Music/Bitwig/Projects/fushigi'))
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--project-folder', action='append', help='Relative project folder; repeat as needed')
    ap.add_argument('--projects-only', action='store_true', help='Inspect states without rehashing audio')
    args = ap.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    if not root.is_dir() or out == root or root in out.parents:
        raise ValueError('Input must exist; output must be outside the source tree')
    out.mkdir(parents=True, exist_ok=True)
    (out / 'states').mkdir(exist_ok=True)
    audio, projects, errors = [], [], []
    for p in ([] if args.projects_only else sorted(root.rglob('*.wav'))):
        if 'spectral' not in p.name.lower():
            continue
        row = {'path': str(p.relative_to(root)), 'bytes': p.stat().st_size,
               'sha256': digest(p)}
        try:
            row.update(wav_info(p))
        except ValueError as e:
            row['error'] = str(e)
            errors.append({'path': row['path'], 'error': str(e)})
        audio.append(row)
    folders = args.project_folder or ['synth', 'gold float', '2016/gold float',
                                      'borderlands 1 (sketch)', 'expansion space (ambient suite)']
    for folder in folders:
        if root not in (root / folder).resolve().parents:
            raise ValueError('Project folder must be inside the source root')
        for p in sorted((root / folder).rglob('*.bwproject')):
            b = p.read_bytes()
            row = {'path': str(p.relative_to(root)), 'bytes': len(b),
                   'sha256': hashlib.sha256(b).hexdigest(), 'spectral_states': []}
            strings = [m.group().decode('ascii') for m in re.finditer(rb'[ -~]{5,}', b)]
            row['preset_path_hints'] = sorted(set(s for s in strings if '.fxp' in s))
            row['spectral_audio_path_hints'] = sorted(set(
                s for s in strings if 'spectral' in s.lower() and '.wav' in s.lower()))
            if zipfile.is_zipfile(p):
                with zipfile.ZipFile(p) as z:
                    for member in z.infolist():
                        if not member.filename.startswith('plugin-states/') or member.is_dir():
                            continue
                        with z.open(member) as f:
                            head = f.read(160)
                        if head[:4] != b'CcnK' or head[16:20] != b'LSP1':
                            continue
                        if member.file_size > 16 * 1024 * 1024:
                            raise ValueError('Unexpectedly large Spectral state')
                        state = z.read(member)
                        sha = hashlib.sha256(state).hexdigest()
                        dest = out / 'states' / (sha + '.fxb')
                        if dest.exists() and dest.read_bytes() != state:
                            raise ValueError('State output collision')
                        dest.write_bytes(state)
                        record = {'member': member.filename, 'sha256': sha, 'bytes': len(state),
                                  'saved_as': str(dest.relative_to(out)),
                                  'kind': head[8:12].decode('ascii')}
                        try:
                            if head[8:12] != b'FBCh' or len(head) != 160:
                                raise ValueError('Unsupported bank header')
                            n = struct.unpack('>I', head[156:160])[0]
                            if 160 + n != len(state):
                                raise ValueError('Bank chunk length mismatch')
                            # Observed host banks start with an uncompressed SYG0
                            # global-settings section, unlike packed FPCh presets.
                            # Validate the entire section stream; never guess past
                            # an unsupported container or silently accept leftovers.
                            if state[160:164] == b'0GYS':
                                raw = state[160:]
                                record['encoding'] = 'uncompressed_bank_sections'
                            else:
                                raw = linplug_fxp.section_bytes({'plugin_id': 'LSP1', 'chunk': state[160:]})
                                record['encoding'] = 'packed_preset_sections'
                            sections = linplug_fxp.split_sections(raw)
                            record['decoded_sha256'] = hashlib.sha256(raw).hexdigest()
                            record['section_tags'] = [tag for tag, _ in sections]
                            record['decoded_bytes'] = len(raw)
                        except (ValueError, IndexError, struct.error) as e:
                            record['decode_error'] = str(e)
                        row['spectral_states'].append(record)
            projects.append(row)
    groups = collections.defaultdict(list)
    for row in audio:
        groups[row['sha256']].append(row)
    unique = [v[0] for v in groups.values()]
    states = [s for p in projects for s in p['spectral_states']]
    summary = {'audio_files': len(audio), 'unique_audio_files': len(unique),
               'bytes': sum(r['bytes'] for r in audio),
               'unique_duration_seconds': sum(r.get('duration_seconds', 0) for r in unique),
               'folders': dict(collections.Counter(str(Path(r['path']).parent) for r in audio)),
               'projects_inspected': len(projects), 'spectral_state_occurrences': len(states),
               'unique_spectral_states': len({s['sha256'] for s in states}),
               'errors': errors}
    report = {'schema_version': 1, 'source_root': str(root), 'summary': summary,
              'audio': audio, 'projects': projects,
              'duplicates': [[r['path'] for r in g] for g in groups.values() if len(g) > 1],
              'limitation': 'Filename candidates only; audio-to-state/MIDI/FX matches not established.'}
    (out / 'inventory.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(summary, indent=2))
    printed = 0
    for p in projects:
        for s in p['spectral_states']:
            if printed < 10:
                print('STATE', p['path'], s['sha256'][:12], s.get('decode_error', 'sections decoded'))
            printed += 1


if __name__ == '__main__':
    main()
