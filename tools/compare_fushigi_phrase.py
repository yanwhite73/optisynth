#!/usr/bin/env python3
"""Render the recovered Gold Float phrase and measure candidate historical bounces.

Run via agent-workload --kind test. Reads original projects/audio as data only.
No original plugin is loaded and no audio is denoised. The candidate match remains
unconfirmed: stochastic phases, historic preset changes and bounce routing are
unknown. Metrics describe the files, not a synth-fidelity score.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

import numpy as np

from extract_bitwig_spectral_phrase import extract

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_wav(path):
    data = path.read_bytes()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError('Expected RIFF/WAVE')
    fmt = samples = None
    pos = 12
    while pos + 8 <= len(data):
        tag, n = struct.unpack_from('<4sI', data, pos)
        body = data[pos+8:pos+8+n]
        if len(body) != n:
            raise ValueError('Truncated WAV chunk')
        if tag == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', body)
        if tag == b'data':
            samples = body
        pos += 8 + n + (n & 1)
    if fmt is None or samples is None or fmt[1] != 2 or fmt[2] != 44100:
        raise ValueError('Expected 44.1 kHz stereo audio')
    if fmt[0] == 1 and fmt[5] == 24:
        b = np.frombuffer(samples, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | b[:, 1] << 8 | b[:, 2] << 16
        x = ((v ^ 0x800000) - 0x800000).astype(np.float64) / 8388608
    elif fmt[0] == 3 and fmt[5] == 32:
        x = np.frombuffer(samples, dtype='<f4').astype(np.float64)
    else:
        raise ValueError('Unsupported audio encoding')
    x = x.reshape(-1, 2)
    if not np.isfinite(x).all():
        raise ValueError('Non-finite audio')
    return x


def features(x):
    # Stereo power avoids cancellation when the patch uses opposing phases.
    n, hop, sr = 8192, 2048, 44100
    window = np.hanning(n)
    power = np.array([np.sum(abs(np.fft.rfft(x[i:i+n] * window[:, None], axis=0))**2, axis=1)
                      for i in range(0, len(x)-n+1, hop)])
    freq = np.fft.rfftfreq(n, 1/sr)
    valid = (freq >= 30) & (freq < 10000)
    midi = np.rint(69 + 12*np.log2(freq[valid]/440)).astype(int)
    chroma = np.stack([power[:, valid][:, midi % 12 == k].sum(axis=1) for k in range(12)], axis=1)
    chroma /= np.maximum(np.linalg.norm(chroma, axis=1, keepdims=True), 1e-30)
    bands = [(20, 200), (200, 2000), (2000, 10000), (10000, 22050)]
    spectrum = power.mean(axis=0)
    fractions = [float(spectrum[(freq >= a) & (freq < b)].sum()/spectrum.sum()) for a, b in bands]
    block = 4410
    envelope = np.array([np.sqrt(np.mean(x[i:i+block]**2)) for i in range(0, len(x)-block+1, block)])
    metrics = {'frames': len(x), 'peak_dbfs': float(20*np.log10(max(abs(x).max(), 1e-15))),
               'rms_dbfs': float(10*np.log10(max(np.mean(x*x), 1e-30))),
               'spectral_centroid_hz': float(np.dot(spectrum, freq)/spectrum.sum()),
               'band_power_fraction_20_200_2000_10000_22050_hz': fractions}
    return metrics, chroma, envelope


def auditions(report_path):
    result = json.loads(report_path.read_text())
    # Choose by the measured pitch-class similarity, retaining the provisional label.
    reference = max(result['bounces'], key=lambda r: r['mean_chroma_cosine_to_candidate'])
    rows = [reference, result['candidate']]
    signals = []
    for row in rows:
        path = Path(row['path'])
        if sha(path) != row['sha256']:
            raise ValueError('Audio changed since comparison')
        x = read_wav(path)
        signals.append(x / np.sqrt(np.mean(x*x)))
    common = 10**(-3/20) / max(abs(x).max() for x in signals)
    outputs = []
    labels = ['original-' + Path(reference['path']).stem.lower(), 'optispectral-candidate']
    for label, signal in zip(labels, signals):
        path = report_path.parent / ('listen-' + label + '.wav')
        data = (signal * common).astype('<f4').tobytes()
        header = b'RIFF' + struct.pack('<I', 36+len(data)) + b'WAVEfmt '
        header += struct.pack('<IHHIIHH', 16, 3, 2, 44100, 44100*8, 8, 32)
        path.write_bytes(header + b'data' + struct.pack('<I', len(data)) + data)
        outputs.append({'path': str(path), 'sha256': sha(path)})
    a, b = [read_wav(Path(r['path'])) for r in outputs]
    if abs(10*np.log10(np.mean(a*a)/np.mean(b*b))) > .00001:
        raise ValueError('Audition levels do not match')
    if max(abs(a).max(), abs(b).max()) > 10**(-3/20) + 1e-7:
        raise ValueError('Audition peak limit exceeded')
    result['auditions'] = {'reference_source': reference['path'], 'files': outputs,
                           'method': 'Equal stereo RMS over entire phrase; common gain gives a maximum peak of -3 dBFS. No other processing.'}
    report_path.write_text(json.dumps(result, indent=2, allow_nan=False)+'\n')
    print(json.dumps(result['auditions'], indent=2))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('folder', type=Path)
    ap.add_argument('--auditions-only', action='store_true', help='Use existing hash-verified comparison audio')
    args = ap.parse_args()
    folder = args.folder.resolve()
    if args.auditions_only:
        auditions(folder/'gold-float-comparison.json')
        return
    graph = json.loads((folder/'gold-project.json').read_text())
    phrase = extract(graph)
    (folder/'gold-float-clips.json').write_text(json.dumps(phrase, indent=2)+'\n')
    track, = phrase['tracks']
    clips = [c for c in track['clips'] if c['kind'] == 'notes']
    clip = clips[0]
    if clip['loop_enabled'] or clip['play_start_beats'] != 0 or clip['content_automation_nodes']:
        raise ValueError('Playback/automation needs further decoding')
    patch = folder/'programs/d951df45fea7-0.ptch'
    output = folder/'gold-float-optispectral-candidate.wav'
    renderer = ROOT/'build/partials-render'
    duration = clip['length_beats']*60/phrase['tempo_bpm']
    command = [str(renderer), str(patch), str(output), '--raw', '--sr', '44100',
               '--seconds', str(duration), '--bpm', str(phrase['tempo_bpm']),
               '--ppq-start', str(clip['start_beats'])]
    for note in clip['notes']:
        # Keep raw velocity in the JSON. These observed values lie on (v-1)/126.
        velocity = 1 + round(note['velocity_normalized']*126)
        command += ['--note', f"{note['pitch']}:{note['start_beats']*60/phrase['tempo_bpm']:.12f}:"
                    f"{note['length_beats']*60/phrase['tempo_bpm']:.12f}:{velocity}"]
    with (folder/'gold-float-render.log').open('w') as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    candidate, c_chroma, c_env = features(read_wav(output))
    result = {'status': 'provisional_phrase_match_not_controlled_fidelity_test',
              'source_project_sha256': graph['sha256'], 'tempo_bpm': phrase['tempo_bpm'],
              'clip_node': clip['node'], 'note_count': len(clip['notes']),
              'patch_sha256': sha(patch), 'renderer_sha256': sha(renderer), 'command': command,
              'candidate': dict(path=str(output), sha256=sha(output), **candidate), 'bounces': [],
              'limitations': ['Saved preset may differ from the patch at historical bounce time.',
                              'No saved snapshot ties these MIDI clips to these earlier audio renders.',
                              'Track has volume automation and a Presswerk send; bounce stage is unknown.',
                              'Free-running random LFOs, phase and prior FX tails are not recoverable.',
                              'Metrics use unmodified audio; no fidelity percentage is inferred.']}
    project = Path(graph['source']).parent
    for path in sorted((project/'bounce').glob('Spectral-bounce-*.wav')):
        met, chroma, env = features(read_wav(path))
        n, m = min(len(chroma), len(c_chroma)), min(len(env), len(c_env))
        row = dict(path=str(path), sha256=sha(path), **met)
        row['mean_chroma_cosine_to_candidate'] = float(np.sum(chroma[:n]*c_chroma[:n], axis=1).mean())
        row['envelope_correlation_to_candidate'] = float(np.corrcoef(env[:m], c_env[:m])[0, 1])
        result['bounces'].append(row)
    (folder/'gold-float-comparison.json').write_text(json.dumps(result, indent=2, allow_nan=False)+'\n')
    print(json.dumps({k: result[k] for k in ['status', 'note_count', 'candidate', 'bounces']}, indent=2))
    auditions(folder/'gold-float-comparison.json')


if __name__ == '__main__':
    main()
