#!/usr/bin/env python3
"""Level comparison for the A/B renders: NAME__original_raw.wav against NAME__partials_raw.wav.

Prints overall RMS in dBFS for each side, the difference, and per-octave-band differences
(partials minus original). The original is a demo-mode render: its demo noise swells to about
-31 dBFS RMS, so overall and band levels of quiet presets read high on that side.

The harmonic columns work through the noise instead (it is never removed or subtracted): for
each played note (C3 0-2.5 s, G3 3-5 s, as rendered by renders/ab_render.sh) they compare the
peak level at each harmonic that stands at least 12 dB over the original's local spectrum
floor, and report the difference of the summed harmonic power and the number of harmonics
used. "low" is the same over harmonics 1-8 only (the body of the sound).
"""
import glob
import os
import sys

import numpy as np


def read(path):
    """Mono mix of a 32-bit float or 16/24-bit PCM WAV (both renderers write float)."""
    d = open(path, 'rb').read()
    if d[:4] != b'RIFF' or d[8:12] != b'WAVE':
        raise ValueError(f'{path}: not a WAV file')
    i, fmt, ch, bits = 12, None, 2, 32
    while i + 8 <= len(d):
        cid, size = d[i:i + 4], int.from_bytes(d[i + 4:i + 8], 'little')
        body = d[i + 8:i + 8 + size]
        if cid == b'fmt ':
            fmt, ch, bits = int.from_bytes(body[0:2], 'little'), int.from_bytes(body[2:4], 'little'), int.from_bytes(body[14:16], 'little')
            if fmt == 0xFFFE:  # extensible: the sub-format is in the first two bytes of the GUID
                fmt = int.from_bytes(body[24:26], 'little')
        elif cid == b'data':
            if fmt == 3 and bits == 32:
                x = np.frombuffer(body[:len(body) // 4 * 4], '<f4').astype(np.float64)
            elif fmt == 1 and bits == 16:
                x = np.frombuffer(body[:len(body) // 2 * 2], '<i2').astype(np.float64) / 32768
            elif fmt == 1 and bits == 24:
                b = np.frombuffer(body[:len(body) // 3 * 3], np.uint8).reshape(-1, 3).astype(np.int32)
                v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
                x = np.where(v >= 1 << 23, v - (1 << 24), v) / 8388608
            else:
                raise ValueError(f'{path}: format {fmt}, {bits} bits')
            return x[:len(x) // ch * ch].reshape(-1, ch).mean(axis=1)
        i += 8 + size + (size & 1)
    raise ValueError(f'{path}: no data chunk')


def db(v):
    return 10 * np.log10(v + 1e-20)


BANDS = [63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000]


def bands(x, sr=48000):
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / sr)
    return [db(spec[(f >= c / np.sqrt(2)) & (f < c * np.sqrt(2))].sum()) for c in BANDS]


NOTES = [(48, 0.2, 2.4), (55, 3.2, 4.9)]  # MIDI note, segment start and end (s)


def harmonic_diff(a, b, sr=48000):
    """(summed harmonic power difference, same over harmonics 1-8, harmonics used)."""
    pa_all = pb_all = pa_low = pb_low = 0.0
    used = 0
    for note, t0, t1 in NOTES:
        seg = slice(int(t0 * sr), int(t1 * sr))
        xa, xb = a[seg], b[seg]
        if len(xa) < sr // 2:
            continue
        w = np.hanning(len(xa))
        sa, sb = np.abs(np.fft.rfft(xa * w)) ** 2, np.abs(np.fft.rfft(xb * w)) ** 2
        f = np.fft.rfftfreq(len(xa), 1 / sr)
        f0 = 440 * 2 ** ((note - 69) / 12)
        for k in range(1, 200):
            fk = k * f0
            if fk > sr / 2.2:
                break
            near = (f > fk * 0.985) & (f < fk * 1.015)
            ring = ((f > fk * 0.94) & (f < fk * 0.97)) | ((f > fk * 1.03) & (f < fk * 1.06))
            if not near.any() or not ring.any():
                continue
            pa, pb, floor = sa[near].max(), sb[near].max(), np.median(sa[ring])
            if pa > floor * 10 ** 1.2:
                used += 1
                pa_all, pb_all = pa_all + pa, pb_all + pb
                if k <= 8:
                    pa_low, pb_low = pa_low + pa, pb_low + pb
    if not used:
        return float('nan'), float('nan'), 0
    low = db(pb_low) - db(pa_low) if pa_low > 0 else float('nan')
    return db(pb_all) - db(pa_all), low, used


def main(folder):
    print(f"{'preset':30} {'orig':>6} {'ours':>6} {'diff':>6} {'harm':>6} {'low':>6} {'n':>4}   " + ' '.join(f'{b:>5}' for b in BANDS))
    for o in sorted(glob.glob(os.path.join(folder, '*__original_raw.wav'))):
        name = os.path.basename(o)[:-len('__original_raw.wav')]
        p = os.path.join(folder, name + '__partials_raw.wav')
        if not os.path.exists(p):
            continue
        a, b = read(o), read(p)
        n = min(len(a), len(b))
        a, b = a[:n], b[:n]
        ra, rb = db(np.mean(a ** 2)), db(np.mean(b ** 2))
        ba, bb = bands(a), bands(b)
        hd, hl, hn = harmonic_diff(a, b)
        print(f'{name[:30]:30} {ra:6.1f} {rb:6.1f} {rb - ra:+6.1f} {hd:+6.1f} {hl:+6.1f} {hn:4d}   '
              + ' '.join(f'{y - x:+5.0f}' for x, y in zip(ba, bb)))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else 'renders/ab')
