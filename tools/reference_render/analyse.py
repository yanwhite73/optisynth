#!/usr/bin/env python3
"""Measurements for the Arturia/Optisynth reference comparison (request 66).

analyse.py alias <wav> <midi>      inharmonic (alias + noise) energy relative to harmonics
analyse.py spread <wav> <count> <period_s> <length_s>   f0 of sequential notes, in cents
Reads 32-bit float or 16/24-bit PCM WAV (left channel). Numpy only.
"""
import json
import struct
import sys
import numpy as np


def read_wav(path):
    data = open(path, 'rb').read()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise SystemExit('not a WAV file')
    pos, fmt, samples = 12, None, None
    while pos + 8 <= len(data):
        tag, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if tag == b'fmt ':
            code, channels, rate, _, _, bits = struct.unpack('<HHIIHH', body[:16])
            if code == 0xFFFE:
                code = struct.unpack('<H', body[24:26])[0]
            fmt = (code, channels, rate, bits)
        elif tag == b'data':
            samples = body
        pos += 8 + size + (size & 1)
    code, channels, rate, bits = fmt
    if code == 3 and bits == 32:
        x = np.frombuffer(samples, dtype='<f4')
    elif code == 1 and bits == 16:
        x = np.frombuffer(samples, dtype='<i2') / 32768.0
    elif code == 1 and bits == 24:
        b = np.frombuffer(samples, dtype=np.uint8).reshape(-1, 3)
        x = ((b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8) | (b[:, 2].astype(np.int32) << 16)) << 8 >> 8) / 8388608.0
    else:
        raise SystemExit(f'unsupported WAV format {fmt}')
    return x.reshape(-1, channels)[:, 0].astype(np.float64), rate


def spectrum(x, rate):
    window = np.hanning(len(x))
    s = np.abs(np.fft.rfft(x * window)) ** 2
    return s, np.fft.rfftfreq(len(x), 1 / rate)


def refine_f0(s, freqs, guess):
    # Choose the f0 near the guess that maximises energy at the first 8 harmonics.
    best, best_f = -1, guess
    for f in np.linspace(guess * 0.97, guess * 1.03, 601):
        idx = np.round(np.arange(1, 9) * f / (freqs[1] - freqs[0])).astype(int)
        idx = idx[idx < len(s)]
        e = s[idx].sum()
        if e > best:
            best, best_f = e, f
    return best_f


def alias(path, midi):
    x, rate = read_wav(path)
    seg = x[int(0.5 * rate):int(1.3 * rate)]
    s, freqs = spectrum(seg, rate)
    f0 = refine_f0(s, freqs, 440 * 2 ** ((midi - 69) / 12))
    bin_hz = freqs[1] - freqs[0]
    harmonic = np.zeros(len(s), bool)
    k = np.arange(1, int(rate / 2 / f0) + 1)
    for h in k * f0:
        width = max(4 * bin_hz, h * 0.002)
        harmonic |= np.abs(freqs - h) <= width
    band = (freqs >= 20) & (freqs <= 16000)
    e_h = s[band & harmonic].sum()
    e_i = s[band & ~harmonic].sum()
    below = s[(freqs >= 20) & (freqs < f0 * 0.9)].sum()   # energy below the fundamental: aliasing folds here
    return {'file': path, 'f0_hz': round(float(f0), 3), 'inharmonic_db': round(float(10 * np.log10(e_i / e_h)), 2),
            'below_f0_db': round(float(10 * np.log10(max(below, 1e-30) / e_h)), 2),
            'rms': round(float(np.sqrt(np.mean(seg ** 2))), 5)}


def spread(path, count, period, length):
    x, rate = read_wav(path)
    cents = []
    for i in range(count):
        a, b = int((i * period + 0.1) * rate), int((i * period + length - 0.05) * rate)
        s, freqs = spectrum(x[a:b], rate)
        f0 = refine_f0(s, freqs, 440.0)
        cents.append(1200 * np.log2(f0 / 440.0))
    c = np.array(cents)
    return {'file': path, 'cents': [round(float(v), 2) for v in c], 'range_cents': round(float(c.max() - c.min()), 2),
            'std_cents': round(float(c.std()), 2)}


if __name__ == '__main__':
    if sys.argv[1] == 'alias':
        print(json.dumps(alias(sys.argv[2], int(sys.argv[3]))))
    elif sys.argv[1] == 'spread':
        print(json.dumps(spread(sys.argv[2], int(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5]))))
