# Classic pad synth architecture — circuit-level notes

Request 51, 19 November 2026. These notes come from schematics and service
manuals, not from parameter lists. This document is a work in progress: the
Prophet-5 section is done, the Oberheim sections are still to read.

## Sources downloaded (local, git-ignored)

The PDFs are in `docs/references/classic_poly/`, downloaded from
[synthfool](https://synthfool.com/docs/). All are scans with no text layer, so
they have to be read as page images.

| File | Pages | SHA-256 (first 16 hex digits) |
| --- | --- | --- |
| Prophet5_Technical_Manual.pdf | 134 | f947fb5ef6d67bca |
| OB8_Service_Manual.pdf | 25 | 467860c061d5ccdc |
| OBXa_Schematics_1st_Ed.pdf | 19 | c9ce3342a4a7380674 |
| OBXa_Service_Manual_1st_Ed.pdf | 13 | 6a5e82d25d314e8b |
| OBX_Service_Manual.pdf | 20 | 27c4a5637b7614e5 |
| SEM_1A_Schematics.pdf | 10 | 604cca6587bda6f1 |
| SEM_Manual_LowRes.pdf | 20 | b35ee6521704c7ce |

The Xpander service manual and schematics were already in `docs/references/`.

## Prophet-5 Rev 3

**Source:** MAME `src/mame/sequential/prophet5.cpp` at revision `3e1d37b9f9`
(BSD-3-Clause). It transcribes the Rev 3.0 technical-manual schematics with
component designators. Values below are quoted from that file; the schematic
pages themselves have not been re-checked.

### Per voice (five identical voice cards)

**Oscillators.** Two CEM3340 VCOs.
- Each voice's pitch is the shared "sum CV" plus a per-voice
  sample-and-hold CV from a single multiplexed 14-bit DAC.
- Oscillator A: saw and pulse, switched and mixed into a CA3280 OTA that sets
  its level.
- Oscillator B: saw, pulse and triangle. It can hard-sync A, and has a "lo freq"
  mode in which it works as a per-voice LFO.
- In the mixer, the saw goes to the OTA's + input and the pulse to its − input.
  Turning both waveforms on therefore sums a saw with an *inverted* pulse. This
  is a characteristic Prophet tone detail.

**Noise.** One shared white-noise source (MM5837) is fed into every voice's
filter input.

**Filter.** CEM3320 4-pole OTA low-pass, with resonance set by a resistor
network (R4414/R4416/R4415).
- The output passes a ~1 Hz DC-block, then a fixed ×3.4 output gain.
- Every cutoff control (shared CV, per-voice keyboard tracking, envelope,
  poly-mod) is summed by an op-amp with a **~751–982 Hz low-pass**. Cutoff
  modulation is therefore smoothed in the analog domain, never stepped.

**Envelopes.** Two CEM3310 ADSRs (filter and amp), using Rx 24.3 kΩ and
Cx 0.039 µF, as in the datasheet example.
- Each stage is an RC curve whose time constant is Rx·Cx·exp(−CV/VT), so the
  time control is exponential.
- The attack charges toward a target *above* the peak and switches to decay on
  reaching the peak. That gives the attack a convex RC shape rather than a
  straight line.
- Decay and release are exponential approaches to their targets.

**Poly-mod.** A per-voice mix of the filter envelope (through a VCA) and
oscillator B (through a VCA, with its waveforms mixed differently from the
audio path). The mix can be routed to:
- oscillator A frequency (through 301 kΩ);
- oscillator A pulse width (30.1 kΩ);
- the filter cutoff (54.9 kΩ).

This is audio-rate FM and filter modulation, separately for each voice.

**Output.** A CA3280 VCA, driven only by the amp envelope.

### Shared by all voices

- **One LFO.** It is another CEM3340, with triangle, saw and square mixed in.
  Pink noise (low-passed at ~159 Hz, then clipped) is mixed with it into a
  "wheel-mod" bus. The mod wheel sets the amount, and the bus feeds osc A/B
  frequency, both pulse widths and the filter. **All voices share one LFO, in
  phase.**
- **Glide/unison.** One OTA slew generator: linear, then RC near the target.
- **Smoothing.** The master "sum CVs" are each low-passed at ~159 Hz. Pitch bend
  is low-passed at ~15.9 Hz.

### Why it sounds alive

- **Every voice is slightly different.** MAME applies per-voice tolerance jitter
  to the oscillator timing capacitors (5%) and resistors (1%), and to the
  filter capacitor (5%). Real units also drift thermally between autotune runs.
- **Several analog nonlinearities colour the sound:** the OTA mixers, the CEM3320
  stages, and the summing points. MAME's CEM3320 drive value is "settled ...
  after experimentation" and still needs measuring against a real unit.

## What this means for Optisynth's pad voice

1. Copy the per-voice/shared split. Each voice gets its own oscillators, filter,
   two envelopes and poly-mod. Shared parts are one LFO/noise wheel-mod bus,
   one glide and one noise source.
2. Give each voice fixed small offsets for oscillator scale and filter cutoff,
   plus slow random drift.
3. Smooth cutoff modulation at about 1 kHz instead of stepping it per block.
4. Use RC-shaped envelope stages with exponential time CVs, including the
   overshoot-target attack.
5. Mix saw and pulse with opposite polarity, and feed noise into the filter.
6. Make poly-mod audio-rate per voice: oscillator B and the filter envelope
   modulate oscillator A pitch, pulse width and cutoff.

## Still to do

- **Read the Oberheim manuals:**
  - SEM: discrete 12 dB state-variable filter, VCOs, envelopes.
  - OB-X: voice card.
  - OB-Xa and OB-8: CEM3320 in 2-pole/4-pole configurations, per-voice pan,
    voice cards.
- **Xpander voice card:** the local manual plus MAME's `xpander_vb.cpp` and
  `cem3372.cpp`.
- **Prophet-5 Rev 2:** the SSM chips need separate study.
- **On decompiling Arturia:** not pursued. The schematics, MAME and the
  datasheets are better and freely readable sources, and Arturia's licence
  terms on reverse engineering have not been checked.
