# Mala: the open Beads design in the rack

Programme item 1 ([MODULE_PROGRAMME.md](../../project/MODULE_PROGRAMME.md)).
Requests 136–140, 26 November 2026; delivered in 0.0.36 on 27 November. Display
name **Mala** (request 140). Saved module id `fx.granular_tape`.

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

The [fidelity assessment and three playable manual studies](MALA_FIDELITY.md) compare the source, test the player-facing differences and give patch recipes.

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
| 250 ms of silence after a quality or mono change; no change while frozen | Button-held Freeze blocks the quality change. Gate-held Freeze permits the change and mutes output for 250 ms (see assessment) |
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

## Playing it: what makes Beads sound good (request 149)

Beads has four uses:
- a granular processor;
- a tempo-synced delay, with Size fully clockwise;
- a reverb;
- a granular wavetable synth, when nothing is patched in.

Two knob laws in the upstream code decide whether the grains sound smooth or broken:

- **Shape** (`grain_envelope.h`):
  - 0: rectangular grains, which click.
  - About 0.33: an instant attack and a falling slope.
  - About 0.66: symmetric, smooth grains.
  - 1: a slow attack and a quick release.
- **Size** (`granular_player.h`): duration is 4 s × 2^(−9 × shortness).
  - 0.6 gives about 90 ms grains.
  - 0.7 gives about 225 ms.
  - 0.8 gives about 0.6 s.
  - 0.9 gives about 1.5 s.
  - Below 0.33, grains play backwards.

| To get | Settings |
| --- | --- |
| Slow, smooth clouds | Shape 0.6–0.7; Size 0.75–0.9; Density 0.65–0.8 (random grains, about 1–7 per second); Time 0.1–0.4; little or no random Shape or Pitch |
| Octave shimmer | As above, with Pitch on the +12 notch, Feedback 0.4–0.6 and Reverb about 0.5 |
| Held harmony | Freeze over sustained or ringing material, then keep playing dry over it |
| Tempo-synced echoes | Size fully clockwise, a clock into Seed; Density picks the subdivision and Time the multiple (the *Copper Relay* study, request 148) |
| Deliberately broken stutter | Shape below 0.35, Size below 0.6, a fast clock into Seed, random amounts up |

Tonal, ringing sources give the grains something to sustain. Short grains on dry percussion mostly catch transients and gaps.

**Pure-tone check (Mala itself is clean).** A 440 Hz sine was run through each quality at unity input gain:

| Path | Quality | Energy outside the tone |
| --- | --- | --- |
| Dry | Bright digital, Sunny tape | −105 dB |
| Dry | Cold digital | −77 dB, as a 12-bit medium should give |
| Dry | Scorched cassette | −30 dB, its 8-bit μ-law tape character |
| Grains at Shape 0.66, Size 0.6 | All four | −40 dB, and −65 dB above 2 kHz (Scorched cassette −31 dB) |
| Grains at Shape 0.3 | All four | −11.5 dB |

## Showcase: Handpan drift (requests 145–149)

Factory 41 replaces Grain circle, whose harsh sound came from its settings (below). A steel tuned-percussion voice improvises in D minor, the Kurd layout of a handpan:
- **Track 1** random-walks the scale in eighths, with rests by chance and occasional flams.
- **Track 2** places the low ding.
- **Track 4** holds Freeze and sets the grains to exact notches.

Mala settings:
- Shape 0.66 and Size 0.84 (grains of about 0.85 s).
- Density 0.74.
- Time 0.22, with a slow random drift.
- No random Shape or Pitch.
- Sunny tape, which rolls off the octave grains' top end.
- Input gain at unity, with the voice entering about 4 dB below full scale.

The 16-bar arc runs: cloud, octave shimmer, Freeze holding the harmony, a fifth over the frozen cloud, then cloud again. Over 100 s:

| Section | Output (dB RMS) | Mala alone | Voice dry |
| --- | --- | --- | --- |
| Cloud | −30.9 / −29.8 | −30.5 / −28.1 | −32.7 / −31.9 |
| Octave | −30.3 / −29.2 | −29.4 / −28.2 | −32.1 / −31.5 |
| Freeze | −29.9 / −30.0 | −28.5 / −28.8 | −31.9 / −32.8 |
| Fifth, frozen | −30.0 / −30.8 | −29.3 / −29.3 | −32.5 / −32.3 |

- No samples clip. The native render `Handpan drift.wav` measures −29.9 dB RMS and peaks at −12.1 dBFS.
- Except in the octave and fifth sections, Mala's share of energy above 4 kHz stays within 3 dB of the dry voice's or below it. The octave sections are brighter, as an octave shift implies.
- Counts of high-frequency transients with Mala are at most 1.5 times the dry voice's, and lower in most sections.

**Why Grain circle sounded broken.**
- Its Shape swept from 0.22 to 0.82 with random variation on top, so many grains had abrupt or rectangular envelopes.
- Its grains were about 25–600 ms, and reversed whenever Size fell below 0.33.
- Sixteenth-note seeds, tape flutter and dry percussion were added on top.

Each choice was measured for level and balance, but together they put Mala at its "deliberately broken" end.

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
