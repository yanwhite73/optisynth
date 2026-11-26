# Mala: the open Beads design in the rack

Programme item 1 ([MODULE_PROGRAMME.md](../../project/MODULE_PROGRAMME.md)).
Requests 136–140, 26 November 2026. Display name **Mala** (request 140). Saved
module id `fx.granular_tape`.

## What it is

Mala is a granular texture processor, delay and reverb. It runs the DSP of the
open-source Mutable Instruments Beads (Émilie Gillet, MIT licence) unchanged,
inside a desktop host that does the job of the original's firmware main loop and
hardware.

- **Source.** The eurorack repository at revision
  `f4cfa7697095a28d292bff758beb51deb49b5a86`.
  - Fetched with `tools/fetch_mutable.py`, with SHA-256 receipts in the review area.
  - Every copied file is recorded in `third_party/mutable/PROVENANCE.json`.
- **Compiled as published** (`third_party/mutable/beads`):
  - the granular processor in all four qualities;
  - the delay controller and delay clock;
  - the grain players, grain envelopes and multitap delay;
  - the trigger generator;
  - the reverb, limiters, wow and flutter and μ-law;
  - the no-input wavetable synth and its tables.
- **Added from stmlib:** six headers (converters, hysteresis, gate flags, delay line).
- **Read in full before use:**
  - the processor, granular processor, parameter and trigger code;
  - `beads.cc`, `cv_reader.*`, `ui.cc`, `settings.*`, `gain_control.*` and the
    normalisation detector;
  - the [manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/).

## The host (src/modular/granular_tape.cpp)

Each item below is a transcription of upstream behaviour, cited in the code.

| Upstream | Mala |
| --- | --- |
| The codec runs at the quality's rate: Bright 48, Cold 32, Sunny 48 (24 inside), Scorched 24 kHz. One block every 1/4000 s | The DSP runs at that same rate and block size. The rack's 48 kHz is converted with stmlib's converters and Beads' own FIR tables (ratios 2 and 3). The bridge adds 12 samples of latency |
| Pot laws (`cv_reader.h`): normal; attenurandomizer with a centre dead band; pitch with virtual notches at 0, 1, 3, 4, 7, 12 and 24 semitones. One-pole 0.05 and hysteresis | Identical, at the block rate. The panel shows pitch in semitones from the same law |
| CV calibration defaults (`settings.cc`) | Rack control 1.0 = 5 V: Time/Size/Shape ±1.6, Pitch 12 st/V, Density and Assign ±1. CV is read once per block, as the ADC is |
| Normalisation probes (`normalization_detector`) | The runtime reports patched sockets (new `AudioModule::patched`). An unpatched CV socket turns its attenurandomizer into the internal random source |
| Mono when one audio input is patched; synth mode when neither is | Same rules. **Difference:** the hardware waits 10 s before entering synth mode. Rack patching is explicit, so Mala switches when the patch is built |
| Automatic input gain with a 1 s settle and 5 s learning, or manual gain (`gain_control.cc`) | Ported from 32-bit codec frames to floats, with the logic and constants unchanged. Input gain 0 = Auto |
| Seed input delayed by 2.5 ms so CV can settle | Same (ten blocks) |
| 250 ms of silence after a quality or mono change; no change while frozen | Same |
| Assign CV: Feedback, Dry/wet or Reverb, or custom amounts (default 255 / 0 / 128) | Same defaults. Turning a custom amount selects Custom, as holding Assign does |
| Gate output on R (Assign + Seed) | The "Grain gate on R" switch |

### Interface (request 137)

The card follows the original's arrangement. It is drawn by us and uses no artwork,
logo or name from the original.
- **Top row:** a lit Freeze button, a large Density knob, a lit Seed button.
- **Second row:** large Time and Pitch knobs, with the Quality button between
  them. Its light uses the hardware's four colours and dims with the buffer
  overwrite reserve.
- **Third row:** Feedback, Size, Shape, Dry/wet.
- **Fourth row:** Assign, the four attenurandomizers, Reverb.
- **Lights:** Feedback, Dry/wet and Reverb each have an assignment light. An
  input-level light sits between In L and In R.
- **Seed gestures:**
  - Pressing Seed unlatches and plays grains.
  - Freeze while holding Seed relatches.
  - The latched Seed button glows slowly.
- **Hidden functions row.** Button-hold settings appear as visible controls:
  Latched seeds, Grain gate on R, Input gain, and the custom assign amounts.

**Sockets:** In L and In R are the hardware's mono inputs. Mala also has a stereo
**In** for the rack's stereo cables, because the rack requires channel counts to
match. Either side patched counts as patched. Output is one stereo plate.

## Checks

`tests/granular_tape_checks.h` and the native run in `app/RackEditor.h` check
what the manual documents:
- **Laws:** the pitch notches and the attenurandomizer dead band.
- **Dry level:** fully dry output is 0.75 × input.
- **Density:** at 12 o'clock, no grains; clockwise, grains.
- **Freeze** holds the material after the source stops.
- **Synth mode** with nothing patched.
- **Bandwidth:** Bright, Cold and Scorched narrow it in order.
- **Gated seeds:** a seed trigger plays one grain only when it arrives.
- **Stereo separation.**
- **Rack:** the runtime's patch report reaches the module.
- **Audio thread:** no allocation.
- **Cost:** 0.5–0.9 % of one core per instance in an optimised build.
- **Interface:** every parameter has a control, inside the face with no overlap,
  at five densities.
- **Buttons and gestures:** they reach the same parameters the patch saves, and
  survive save and recall.

**Seed rain** (factory 40) plays the Optical vapour reed chord through Mala. Its
offline render is `Seed rain.wav` in the native review folder:
- one four-bar pass at 80 BPM, then a 6 s tail;
- about −30 dB RMS, peak −12 dBFS;
- a wide stereo image from the randomised grain panning.

## Not reproduced

- Calibration mode and the factory test.
- Backup of a frozen buffer across power cycles.
- The advanced firmware options that the release firmware leaves off: monophonic
  mode, and Trim.
- The exact LED animations.
- Synth mode is not compared sample by sample against hardware. That would need
  recordings.

## Licence and name

The upstream files are MIT and keep their notices. The repository asks that the
"Mutable Instruments" name not appear on derivative works, and advises against
keeping the original module name. "Mala" (a string of prayer beads) meets both.
The internal id is neutral, so a rename touches no saved patches.
