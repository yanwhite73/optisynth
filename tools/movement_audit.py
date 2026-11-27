#!/usr/bin/env python3
"""Measure how much each movement assignment changes a patch (request 146).

Reads the folder written by `Optisynth --movement-audit "<preset>" "<folder>"`.
For each route it compares the two values the movement reaches (low/high) and the
ends of the parameter's range (min/max): overall level and four band levels.
Prints a table and writes movement-report.json beside the renders.
"""
import argparse
import json
from pathlib import Path

import numpy as np
import soundfile as sf

BANDS = [(35, 250), (250, 950), (950, 3200), (3200, 12000)]
SR = 48000


def bands(x):
    """RMS level in dB, overall and per band, of a mono signal."""
    spectrum = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    freqs = np.fft.rfftfreq(len(x), 1 / SR)
    total = 10 * np.log10(spectrum[(freqs >= 35) & (freqs < 12000)].sum() + 1e-20)
    per = [10 * np.log10(spectrum[(freqs >= lo) & (freqs < hi)].sum() + 1e-20) for lo, hi in BANDS]
    centroid = (spectrum * freqs).sum() / (spectrum.sum() + 1e-20)
    return total, np.array(per), centroid


def mono(path):
    x, sr = sf.read(path, dtype='float64')
    assert sr == SR
    return x.mean(axis=1) if x.ndim == 2 else x


def compare(a, b):
    ta, pa, ca = bands(a)
    tb, pb, cb = bands(b)
    band = pb - pa
    return {
        'level_db': round(float(tb - ta), 2),
        'band_db': [round(float(v), 2) for v in band],
        'centroid_change': round(float(cb / ca - 1), 3),
        # Largest change in any part of the spectrum, level included.
        'largest_db': round(float(max(abs(tb - ta), np.abs(band).max())), 2),
    }


def verdict(db):
    return 'inaudible' if db < .5 else 'subtle' if db < 1.5 else 'clear' if db < 4 else 'strong'


def windows(x, seconds=.5):
    n = int(SR * seconds)
    return [x[i:i + n] for i in range(0, len(x) - n + 1, n)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    args = parser.parse_args()
    folder = args.folder
    audit = json.loads((folder / 'audit.json').read_text())
    rows = []
    for route in audit['routes']:
        tag = f"{route['index']:02d}"
        swing = compare(mono(folder / f'{tag}-low.wav'), mono(folder / f'{tag}-high.wav'))
        span = compare(mono(folder / f'{tag}-min.wav'), mono(folder / f'{tag}-max.wav'))
        rows.append({**route, 'swing': swing, 'range': span,
                     'swing_verdict': verdict(swing['largest_db']), 'range_verdict': verdict(span['largest_db'])})
    moving, still = mono(folder / 'as-saved.wav'), mono(folder / 'still.wav')
    # Movement over time: the largest band difference in each half-second window.
    over_time = [compare(s, m)['largest_db'] for m, s in zip(windows(moving), windows(still))
                 if np.sqrt((s ** 2).mean()) > 1e-4]
    summary = {
        'median_window_db': round(float(np.median(over_time)), 2) if over_time else 0,
        'p90_window_db': round(float(np.percentile(over_time, 90)), 2) if over_time else 0,
        'max_window_db': round(float(np.max(over_time)), 2) if over_time else 0,
    }
    (folder / 'movement-report.json').write_text(json.dumps({'routes': rows, 'as_saved_vs_still': summary,
                                                             'overrides': audit.get('overrides', [])}, indent=1))
    print(f"{'Route':28} {'base':>6} {'reach':>13} {'rate':>7}  {'swing dB':>8} {'':10} {'full range dB':>13}")
    for r in rows:
        reach = f"{r['low']:.3g}..{r['high']:.3g}"
        rate = f"{r['lfo_rate_hz']:.3g}Hz" if r['lfo_rate_hz'] else ''
        print(f"{r['node'] + '.' + r['parameter']:28} {r['base']:6.3g} {reach:>13} {rate:>7}  "
              f"{r['swing']['largest_db']:8.2f} {r['swing_verdict']:10} {r['range']['largest_db']:13.2f} {r['range_verdict']}")
    print(f"As saved vs still, per half second: median {summary['median_window_db']} dB, "
          f"90th percentile {summary['p90_window_db']} dB, max {summary['max_window_db']} dB")


if __name__ == '__main__':
    main()
