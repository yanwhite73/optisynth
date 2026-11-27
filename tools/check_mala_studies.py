"""Render the three manual studies and test their defining cables with Optisynth 0.0.37+."""
import argparse
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf


ROOT = Path(__file__).resolve().parents[1]
CABLES = {"Copper Relay": "seed", "Shards in Reverse": "time", "Lantern Choir": "pitch"}


def render(app, preset, directory):
    subprocess.run([str(app), "--movement-audit", str(preset), str(directory), "seconds=16"], check=True)
    samples, rate = sf.read(directory / "as-saved.wav", always_2d=True)
    if rate != 48000 or samples.shape != (16 * rate, 2) or not np.isfinite(samples).all():
        raise ValueError(f"Invalid stereo render for {preset}")
    return samples


def rms(signal):
    return np.sqrt(np.mean(signal ** 2))


def check(app, library, work):
    for name, port in CABLES.items():
        preset = library / f"Mala - {name}.optisynth"
        original = render(app, preset, work / name)
        repeat = render(app, preset, work / (name + "-repeat"))
        if not np.array_equal(original, repeat):
            raise ValueError(f"Recall changed the sound of {name}")
        doc = json.loads(preset.read_text())
        links = [c for c in doc["connections"] if c["to"] == {"node": "mala", "port": port}]
        if len(links) != 1:
            raise ValueError(f"Expected one {port} cable in {name}")
        doc["connections"].remove(links[0])
        alternative = work / f"{name}-unpatched.optisynth"
        alternative.write_text(json.dumps(doc))
        other = render(app, alternative, work / (name + "-unpatched"))
        level = rms(original[48000:])
        delta = rms((original-other)[48000:])
        peak = np.max(np.abs(original))
        if not .02 < peak < .95 or delta < .001 or level <= 0:
            raise ValueError(f"Unexpected level or ineffective {port} cable in {name}")
        db = 20 * np.log10([peak, level, delta/level])
        print(f"{name}: peak {db[0]:.1f} dBFS, RMS {db[1]:.1f} dBFS, disconnect {port} {db[2]:+.1f} dB relative")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, default=ROOT/"app/Builds/MacOSX/build/Release/Optisynth.app/Contents/MacOS/Optisynth")
    parser.add_argument("--library", type=Path, default=ROOT/"patches/mala")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="optisynth-mala-") as folder:
        check(args.app.resolve(), args.library.resolve(), Path(folder))
