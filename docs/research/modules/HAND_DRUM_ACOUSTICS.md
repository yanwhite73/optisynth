# Frame drum and pakhawaj

## What to model

A circular head has spatial vibration patterns. A central blow cannot efficiently excite a mode with a node at the centre. Moving the point of contact changes the individual modal weights. A broad palm also averages the force over a larger area than a fingertip. Increasing velocity, moving toward the rim and making the contact harder are three different gestures. A useful frame drum should preserve these distinctions after the recording has ended. The radial Bessel solutions for a uniform circular head provide a starting point for spatial weighting. [Membrane equations and experimental spectra](https://ed.iitm.ac.in/~raman/drums.pdf)

The pakhawaj has two unlike heads sharing a body. Central loading on the treble head shifts its modal frequencies. Published measurements give the first peak about 7.4% above the fundamental inferred from the second, while the higher peaks lie close to an integer series. A fixed harmonic oscillator bank would miss this. Measurements of a different instrument and stroke are a cross-check on the mechanism, not tuning targets for every sound. [Measured pakhawaj spectrum, table on p. 306](https://ed.iitm.ac.in/~raman/drums.pdf) · [Composite membrane model and measured modes](https://repository.ias.ac.in/39763/)

Avanzini and Marogna model impact force, membrane tension modulation and air coupling separately. Their two-head model exchanges energy through an enclosed volume. A sympathetic head can receive energy and return it as the struck head decays. A practical reduced model can retain that exchange and separate loss times without a full spatial simulation. An open-backed frame drum needs a different passive response from an enclosed barrel. [Membrane percussion model](https://avanzini.di.unimi.it/downloads/publications/avanzini_taslp10.pdf)

Chromaphone separates impact stiffness from force and gives position and coupling their own controls. Collision exposes the hit and listening positions separately. These are interface precedents for keeping the hand, head and body editable. AAS's coupled mode exchanges energy between objects. A parallel reverb cannot substitute for that interaction. [Chromaphone 3 manual, sections 5.3.4–5.3.5](https://www.applied-acoustics.com/chromaphone-3/manual/) · [Ableton Collision](https://www.ableton.com/en/manual/live-instrument-reference/#collision)

Elements gives excitation position and damping separate controls and accepts an external signal directly into its resonator. Kaivo takes that idea further with a recorded granular source feeding a resonator and a vibrating body. For a hand drum the short, irregular contact is a good place to use a recording. The sustained note should still respond when the player presses the head. In this implementation the first 20 ms of a mapped WAV can drive the membrane, and an adjustable amount of the recording is audible alongside it. [Elements manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/elements/manual/) · [Madrona Labs on Kaivo’s signal path](https://madronalabs.com/products/kaivo)

The Shanti Mandir lessons distinguish open and closed strokes and compound strokes. A player needs independent bass and treble lanes that can land together. Keyboard keys on the DMI audition player are identifiers; its author does not give them individual bol labels. Use descriptive playing roles until a performer verifies those labels. [Playing exercises](https://www.shantimandiraustralia.com.au/basic-strokes-and-exercises-pakhawaj) · [DMI recorded strokes](https://dimultimusic.co.id/vst/research/pakhawaj/)

The playing lesson gives two particularly useful comparisons. For *na* and *tin*, the index finger changes its strike point between rim and loaded skin while other fingers touch the head. For *ti* and *din*, staying on the head or rebounding makes the difference between a stopped and a ringing sound. Position and post-impact damping should therefore be independently playable. The present patch exposes a strike position and one pressure control. A second, independently positioned finger damping selected modes remains an acoustic simplification. [Shanti Mandir, right-hand stroke demonstrations](https://www.shantimandiraustralia.com.au/basic-strokes-and-exercises-pakhawaj)

## Recordings

Measured 50 frame hits and 19 Moroccan frame hits from the local Organic Loops pack. Moroccan 09 is a zero-byte file. The numbered filenames provide no velocity, radial position or bol annotations. The frame mapping is an authored instrument made from acoustic matches. DMI provides five recorded stroke groups at soft, medium and hard strengths. Its demo includes room decay. The fits describe the recorded sound and include some of that colour. Public player audio and commercial WAVs stay outside the source distribution.

The analysis tool aligns each hit to onset, inspects early and later spectra, estimates narrow-band decay, compares transient energy across four frequency bands and reports the time containing 90% of the first second's energy. Examples measured on peak-normalised mono signals:

| Reference | Strongest early/body peak | 90% energy time |
|---|---:|---:|
| Frame 15 | 105.5 Hz | 528 ms |
| Frame 04 | 187.5 Hz | 331 ms |
| Frame 05 | 186.0 Hz, strong 472 and 502 Hz components | 233 ms |
| Frame 18 | 218.3 Hz | 36 ms |
| Pakhawaj D1 soft / medium / hard | 156.7 / 158.2 / 162.6 Hz | 289 / 316 / 493 ms |
| Pakhawaj F1 soft / medium / hard | 164.1 / 564.0 / 171.4 Hz | 43 / 207 / 349 ms |

Force can change which mode dominates as well as the decay. The three-strength pakhawaj groups therefore need separate fits. The frame recordings can supply the contact sound directly on the owner's machine. The physical branch must work on its own when those files are unavailable.

## Repeating the comparison

Use Python with NumPy, SciPy and SoundFile. FFmpeg decodes the DMI player files. The filenames below refer to the local research folder under `build`. The analysis script records magnitudes and decay estimates. It writes no sample or phase data into the source tree.

1. Run `tools/analyse_hand_drums.py` with the frame and Moroccan WAVs and all 15 DMI player files. Pass `--out measurements.json --profiles src/modular/hand_drum_profiles.h` to generate the first fit.
2. Build the standalone and run its existing `--snapshot DIRECTORY` entry point. It renders each C/D/E/F/G role at velocities 0.220, 0.565 and 0.910, at the inner strike zone, with the sample bank empty. It also renders both sequenced presets with their saved settings.
3. Run the same analyser on `DIRECTORY/stroke-check/*.wav`, supplying `--out first-pass.json`. Repeat the first fit with `--baseline-renders first-pass.json`. When the wrong line dominates, the fitter matches nearby resolved lines and makes bounded relative gain corrections. This corrects for the spectrum of the finite hand contact and the excitation position.
4. Rebuild, render and measure the corrected strokes. Run `python3 tools/compare_hand_drum_renders.py measurements.json corrected.json` to reproduce the table. T90 is time to 90% of the first second’s energy. It is distinct from a reverberation time or a mode’s exponential decay constant.

The model uses six driven resonances per fitted layer. An additional damped mode exchanges energy with the strongest fitted resonance. It represents a passive body response for the frame and an opposite-head response for the barrel. Each simultaneous strike has its own passive state. A continuous shared air cavity, inter-hand choking, independently positioned damping fingers and repeated measured takes at a fixed force would require further work. The dry pakhawaj uses synthesis throughout. A local recording can be assigned to any cell of either instrument.

## Results from the delivered standalone

The table shows medium strokes with synthesized contact. The summary covers all three forces. These are comparisons against the recordings used for fitting. They measure reconstruction at one position. A blind listening test or held-out player would answer a different question.

| Instrument / stroke | Reference peak (Hz) | Model peak (Hz) | Reference T90 (ms) | Model T90 (ms) |
|---|---:|---:|---:|---:|
| Frame drum / Bass | 105.5 | 105.5 | 528 | 528 |
| Frame drum / Open | 187.5 | 187.5 | 331 | 302 |
| Frame drum / Slap | 290.0 | 291.5 | 210 | 199 |
| Frame drum / Stopped | 218.3 | 218.3 | 36 | 34 |
| Frame drum / Fingers | 137.7 | 137.7 | 184 | 194 |
| Pakhawaj / Bass | 156.7 | 158.2 | 434 | 384 |
| Pakhawaj / Open | 158.2 | 158.2 | 316 | 315 |
| Pakhawaj / Slap | 167.0 | 167.0 | 310 | 298 |
| Pakhawaj / Stopped | 564.0 | 565.4 | 207 | 201 |
| Pakhawaj / Fingers | 174.3 | 168.5 | 479 | 485 |

Frame drum: 13/15 dominant peaks within 5%, median absolute T90 error 9 ms.
Pakhawaj: 15/15 dominant peaks within 5%, median absolute T90 error 3 ms.

The largest remaining peak disagreements are the soft and hard frame stopped strokes. The model emphasises 95 and 88 Hz where the recordings emphasise 135 and 139 Hz. At medium force the stopped pakhawaj improved from 133 to 565 Hz, against a 564 Hz reference. Its bass stroke still loses energy about 50 ms too soon at medium force. A matching frequency and T90 leave room for differences in the attack, partial balance and room sound.

The [reference measurements](hand_drum_measurements/references.json), [first-pass renders](hand_drum_measurements/first-pass.json) and [corrected renders](hand_drum_measurements/corrected.json) preserve the measured spectra, losses, residual band envelopes and durations. The comparison script accepts these files directly.

## 0.0.37: Sympathy and contact area made audible (request 146)

The audit found Sympathy and Contact area within 0.6 dB across their full range. The passive mode only exchanged energy with the main mode. It sat about 70 Hz away from that mode, at a coupling rate near 1.5 Hz, so it received almost nothing. Contact area only reweighted six low modes.

The changes:
- **Sympathy.** A cubic fraction of the contact force now reaches the passive mode through the frame (0.6 of that on the pakhawaj). The energy exchange is kept. The passive mode sounds above the recorded skin, because the recording is the skin alone.
- **Contact area.** It scales contact time by 2^(1.5 × (area − 0.38)) and tilts the recorded skin around 1.2 kHz. Both are neutral at the measured 0.38.

Effect on the defaults:
- **Strike.** The default frame-drum strike changed by at most 0.24 dB with the recorded skin on.
- **Reference recordings.** Re-measured on the 0.0.37 stroke renders:
  - Frame drum: 13 of 15 dominant peaks are within 5%, as before. The median T90 error is 6 ms, against 9 ms in 0.0.35.
  - Pakhawaj: 15 of 15 dominant peaks are within 5%, with a median T90 error of 3 ms, unchanged.

