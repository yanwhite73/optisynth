#!/usr/bin/env python3
"""Extract observed old-format Spectral clips from our read-only graph dump.

Numeric IDs are traced from actual format-0x75 objects; this is not a general
Bitwig importer. Preserve raw velocities and graph node IDs for audit. No DAW,
plugin, network, or source-file writes. Candidate audio links do not establish
which historical patch/MIDI performance created a bounce.
"""
import argparse
import json
from pathlib import Path


def extract(graph):
    objects = graph['objects']
    refs = {o['ref_id']: o for o in objects}

    def obj(link):
        return objects[link['node']] if 'node' in link else refs[link['ref']]

    def val(o, key, default=None):
        return o['fields'].get(str(key), {}).get('value', default)

    def child(o, key):
        return obj(val(o, key))

    def items(o, key):
        return [obj(v) for v in val(o, key, [])]

    root = obj(graph['root'])
    tempo = val(child(child(root, 3033), 529), 712)
    tracks = []
    for track in items(child(root, 1245), 1246):
        devices = [child(slot, 407) for slot in items(child(track, 356), 324)]
        if not any(val(d, 154) == 'Spectral' for d in devices):
            continue
        out = {'node': track['ordinal'], 'name': val(track, 347),
               'devices': [{'node': d['ordinal'], 'name': val(d, 154),
                            'enabled': val(d, 163), 'state_file': val(d, 3045)} for d in devices],
               'clips': [], 'automation_nodes': [], 'mixer_node': child(track, 357)['ordinal']}
        arranger = child(track, 349)
        out['automation_nodes'] = [a['ordinal'] for a in items(arranger, 569)]
        for clip in items(child(arranger, 568), 543):
            content = child(clip, 648)
            playback = child(content, 2447)
            record = {'node': clip['ordinal'], 'name': val(clip, 646),
                      'start_beats': val(clip, 687), 'length_beats': val(clip, 38),
                      'loop_enabled': val(playback, 2449),
                      'play_start_beats': val(child(playback, 2444), 687)}
            if content['class'] == 191:
                record['kind'] = 'notes'
                record['notes'] = []
                for note in items(child(content, 1180), 543):
                    if note['class'] != 102:
                        raise ValueError('Unsupported note class')
                    expression = child(note, 648)
                    if any(val(expression, k) is not None for k in [3123, 3124, 3125, 3122, 5802]):
                        raise ValueError('Note expression requires separate decoding')
                    record['notes'].append({'node': note['ordinal'], 'pitch': val(note, 238),
                                           'start_beats': val(note, 687), 'length_beats': val(note, 38),
                                           'velocity_normalized': val(note, 239)})
                record['content_automation_nodes'] = [a['ordinal'] for a in items(content, 1181)]
            elif content['class'] == 261:
                record['kind'] = 'audio'
                record['events'] = []
                for event in items(child(content, 1180), 543):
                    source = child(child(event, 525), 1204)
                    record['events'].append({'node': event['ordinal'],
                        'start_beats': val(event, 687), 'length_beats': val(event, 38),
                        'source_path': val(child(source, 4766), 3386),
                        'source_seconds': val(source, 149), 'sample_rate': val(source, 148)})
            else:
                raise ValueError('Unsupported clip content class')
            out['clips'].append(record)
        tracks.append(out)
    return {'source': graph['source'], 'source_sha256': graph['sha256'],
            'tempo_bpm': tempo, 'tracks': tracks,
            'limitation': 'Audio links are arrangement references, not proof of a historical preset/performance match.'}


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('graph', type=Path)
    ap.add_argument('output', type=Path)
    args = ap.parse_args()
    if args.graph.resolve() == args.output.resolve():
        raise ValueError('Cannot overwrite graph')
    result = extract(json.loads(args.graph.read_text()))
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
