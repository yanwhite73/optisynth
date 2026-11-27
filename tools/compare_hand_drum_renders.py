"""Compare isolated dry Optisynth strokes to the recordings used to fit the model.

Run analyse_hand_drums.py on the reference files and on the native snapshot
stroke-check WAVs, then pass the two resulting JSON files to this program.
"""
import argparse
import json
import statistics
from pathlib import Path

from analyse_hand_drums import FRAME, PAKH


def compare(reference, renders):
    print("| Instrument / stroke | Reference peak (Hz) | Model peak (Hz) | Reference T90 (ms) | Model T90 (ms) |")
    print("|---|---:|---:|---:|---:|")
    for instrument in ("Frame drum", "Pakhawaj"):
        errors, close = [], 0
        for i, (label, key) in enumerate(zip(("Bass", "Open", "Slap", "Stopped", "Fingers"), (0, 2, 4, 5, 7))):
            for level in range(3):
                name = (f"FRD_FrameDrums_{FRAME[i][0][level]:02d}.wav" if instrument == "Frame drum"
                        else f"{PAKH[i]}1-{level+1}.m4a")
                measured = reference[name]
                rendered = renders[f"{instrument}-{key}-{level}.wav"]
                a, b = measured["modes"][0][0], rendered["modes"][0][0]
                ta, tb = measured["t90_ms"], rendered["t90_ms"]
                close += abs(b/a-1) <= .05
                errors.append(abs(ta-tb))
                if level == 1:
                    print(f"| {instrument} / {label} | {a:.1f} | {b:.1f} | {ta} | {tb} |")
        yield f"{instrument}: {close}/15 dominant peaks within 5%, median absolute T90 error {statistics.median(errors)} ms."


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("renders", type=Path)
    args = parser.parse_args()
    summaries = list(compare(json.loads(args.reference.read_text()), json.loads(args.renders.read_text())))
    print()
    print("\n".join(summaries))
