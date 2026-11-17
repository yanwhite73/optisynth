# LinPlug Spectral 1.2.7: synth architecture

Static analysis, 5–9 November 2026. Companion to `linplug_findings.md`, which covers the
file format, the section tags and the other LinPlug synths (MorphoX, CronoX).

How each claim is known:

- **[host]** Spectral reported it through VST2 (`info` run: names of all 1,241 automatable
  parameters).
- **[decl]** It comes from the module parameter declarations (`tools/schemas/LSP1_params.json`).
- **[decomp]** Read in Ghidra's decompiled output of the named function. Decompiled text stays in
  the session scratchpad; only behaviour is described here, in prose.
- **[data]** Measured over the 2,648-preset corpus.
- **[inferred]** A reasoned reading that no single source confirms.

Spectral is an **additive wavetable synthesiser**. Each oscillator stores two harmonic spectra
(A and B) and a drawn spectral filter curve. When a spectrum or filter is edited, or a preset
loads, Spectral pre-renders band-limited single-cycle tables by summing harmonics. It renders one
unfiltered set, plus one filtered set for every semitone of filter position. Voices play those
tables back and crossfade between them in real time. A resonance band-pass and an optional
classic low-pass then act on each oscillator's output. The result passes through amplitude and
modulation stages and a 6-slot effects chain.

## Signal flow

**Manual evidence added 9 November:** see
[`spectral_archive_inspection_2026-11-09.md`](spectral_archive_inspection_2026-11-09.md).
The newly available manual describes single-voice unison modulation sources, FM high-pass
modes, phase-zero alignment, zero-base matrix-depth routing, LP+ sensitivity and separate
Osc Gain/Volume destinations. These are comparison targets, not implemented corrections.
Its OSC-envelope description conflicts with both another passage and our measurements;
keep the measured results below and test the remaining distinctions explicitly.

```
 MIDI ─► Arp / step sequencer (32 steps) ─► Glide ─► voice allocation (Voices, mono … 32)
                                                        │
 per voice (SpectralTone_c), per oscillator 1..4 (GenSpectral_c) ───────────────────────
   tables (WaveROMSpectral_c, built at edit/load time, not per note):
     spectrum A, spectrum B (256 harmonics, level + phase)
       × filter curve shifted by each semitone level ─► 217 levels × 21 band-limit tiers
   playback, per unison voice:
     phase (pitch: Semi, Cent, Track, glide, matrix, Precision drift)
       ─ X-Mod phase offsets / wrap ─ Symmetry phase warp
     read A and B at the two filter levels around the cutoff position, crossfade the levels,
       then crossfade A↔B by Wave Mix     (filter bypassed: unfiltered A and B only)
     × input gains (amp-envelope stream and UI Osc Volume; matrix distinctions below)
     ─ unison stereo sum / pan ─ LP Type: 1–4 one-pole stages (off at "oo")
     + resonance: nonlinear state-variable band-pass, mixed in by Resonance
       (one voice: input before LP+; spread: input after LP+; see reinspection below)
     × output level/envelope factors ─► output routing
     ─► Route Balance between Fil Route 1 and Fil Route 2
        (each target: an effect slot "FX 1".."FX 6" or OUT)
   × Main envelope, Velocity→Volume, per-voice Main Volume matrix modulation
 ─────────────────────────────────────────────────────────────────────────────────────────
 per-oscillator sends enter seven 2× buffers (FX 1..6 / OUT)
   ─► seven internal Downsample_c stages (2:1) ─► normal-rate FX / OUT buffers
 Fx slot 1 … Fx slot 6 (each: On, Type, Route = a later slot or OUT)
   ─► Main Vol (post effects, measured) ─► Master EQ (VEQ4) ─► Limiter (VL01) ─► stereo out
```

- **Routing [data + decl].** The routing comes from the named preset fields (`GOS0` Fil Route
  1/2 and Route Balance; `SYG3` Fx Route per slot). Each oscillator and filter pair is split
  between two destinations, so oscillators can enter the effect chain at different points. In
  "YW 1 note love", oscillator 1 sends to FX 1 and FX 4, with the balance fully at one end.
- **Cross-modulation [decl/data/manual].** Each oscillator and each filter has two X-Mod slots.
  Oscillator types are AM/FM/PM; filter types are FM/RM. The manual (p25, inspected 9 November)
  identifies filter RM as **resonance modulation**, correcting our earlier ring-modulation
  interpretation. Each slot has a source ("OSC n" or "FIL n") and an amount.
- **No separate FilterSV3 instance in the voice.** The state-variable filter class (`FilterSV3_c`) is
  constructed only by `SpectralMapper_c`, the preset converter. The voice's resonance and
  low-pass are small filters inside the oscillator render instead. **[asm + decomp]**

## Oscillators: additive tables

Read in `WaveROMSpectral_c::calculateWaveComplete`, the constructor and `create`. **[decomp]**

- **Harmonics.** A spectrum is a level array of 257 entries with level and phase. Entry h is
  harmonic h, from 1 to 256; entry 0 is not summed.
- **Table build.**
  - Each output table is one 2048-sample cycle.
  - Every harmonic has a precomputed sine table (`WaveROM4_c::HarmonicsArray[h]`). The build
    reads it at an offset set by the harmonic's phase, scales it by the harmonic's level, and
    adds it in.
  - Harmonics quieter than about −140 dB (linear 10⁻⁷) are skipped.
  - There is no FFT; summing is plain, 8 samples per step.
- **Waveform peak normalisation, measured 9 November.** After summing, each waveform is
  scaled down if its unfiltered full-spectrum peak exceeds one. A and B have independent
  gains; their blend is not renormalised. The same gain remains across filter levels and
  band-limit tiers. Waves below unity are not boosted. This was missing in partials and
  is distinct from normalising the loudest *spectral entry* to 0 dB. See round 3 below.
- **Band-limit tiers (`WRSSetMaxHm`, `WRSSetInc`).** Each level holds **21 tiers** of the same
  sound, built incrementally from the top tier down.
  - The highest harmonic per tier falls by about a third of an octave per tier: 256, 203, 161,
    128, 101, 80, 64, 50, 40, 32, 25, 20, 16, 12, 10, 8, 6, 4, 3, 2, 1.
  - Tables shrink as fewer harmonics are needed. Tiers 0–2 are 2048 samples; tiers 3–5 1024;
    6–8 512; 9–11 256; 12–14 128; 15–20 64. Each smaller table is a decimated copy, every 2nd,
    4th, … 32nd sample.
  - Together a level's 21 tiers take exactly 12,288 floats (the 0x3000 stride).
- **Tier choice at playback (`getPhase`).** For the voice's phase increment, Spectral picks the
  fullest tier whose top harmonic stays below **sample rate ÷ 4.04**, just under a quarter of
  the sample rate. Very high increments are halved until they fit.
  - **Consequence [inferred]:** at 44.1 kHz no partial above about 10.9 kHz is ever played. This
    is a conservative anti-aliasing margin, and it explains part of Spectral's soft top end.
- **Table bank.**
  - Level 0 is unfiltered. Levels 1–216 apply the filter curve at one semitone of shift per
    level (see Filters).
  - There is one bank per spectrum: 217 levels × 12,288 floats, about 10.7 MB each. Each
    oscillator has three banks: A, B and a third, not traced.
- **The "6 tables" are per voice, not per level.** `getWaveformN01` hands a voice six pointers:
  A and B unfiltered, and A and B at each of the two levels around the current filter position.
  An earlier version of this document said "6 tables per level"; that was wrong.
- **When tables are rebuilt.** Only from edit and load paths (`handleSpectrumChange`,
  `GenEditHelper`, `FilEditHelper`, `doFilProcess`, `calculateWave`), never in the audio loop.
- **Wave tools** (the interface's Wave Processing menu) **[tbl]**:
  - spectrum edits: Copy, Paste, Init to Sine, Normalize Levels, Swap Harmonics, Random Spectrum,
    Init Phases to 0;
  - files: Save Wave A+B, and Analyse Wavefile, which imports audio as a spectrum;
  - transforms: `shiftOctave`, `shiftSemitone`, `smoothen`, `invert`, `saturate`,
    `randomSpectrum`. **[sym]**

## Oscillator playback (`GenSpectral_c::renderNX`, `prepProc`) [decomp]

- **Phase.** A per-oscillator phase accumulator runs in table units (2048 per cycle). The
  increment is the note frequency × 2048 ÷ sample rate. Smaller tiers are read with the phase
  scaled down to their length.
- **Start phase.** Start Phase × 2048, or random when Free-Run is set. Unison voices take phase
  offsets from a fixed table of 162 values, used in turn.
- **Unison.** A loop runs over the Spread Voices count. Each voice gets a detune from fixed
  spread-factor tables, scaled by Spread Detune, and a fixed stereo position. Some voice counts
  select alternative factor sets.
- **Symmetry: a two-segment phase warp.** The part of the cycle before the symmetry point is
  mapped to the first half of the table, and the rest to the second half. It is phase
  distortion on the wavetable readout, a pulse-width-like control for any spectrum.
- **Readout.** Linear interpolation between adjacent table samples.
- **Wave Mix.** A linear crossfade between the A and B tables. The control is the `SAW1` Wave
  Mix field (0–100 %, default 50 %), mapped to −1…+1.
- **Cross-modulation.** X-Mod 1 and 2 arrive through `cacheXMod` as phase offsets added to the
  readout position.
- **Gain and level.**
  - Gain passes through a soft curve: g·0.1 + g ÷ (0.0422·g² + 1.111).
  - It is then multiplied by the oscillator's smoothed amplitude and its pan/volume factor.
- **Control rate.** Modulated values (pitch, amplitude, filter position, mix) are updated every
  **16 internal loop iterations** and ramped linearly across each block. Do not equate this
  directly with 16 host samples: the doubled processing loop needs caller/rate tracing
  (see 9 November reinspection below). They arrive as 19 per-oscillator
  modulation streams: pitch, amplitude, symmetry, spread, spread volume, stereo, volume,
  cutoff, resonance, LP offset, pan, route balance and others.

## Filters: spectral curve, resonance and low-pass

**The spectral curve [decomp].**

- **Filter curve.** It is a level array of **241 points, one per semitone**.
- **Harmonic positions.** At set-up, each harmonic gets its pitch in semitones above the
  fundamental. The pitch is rounded up to a whole semitone, with a fraction for interpolating
  between neighbouring curve points; harmonic 256 sits 96 semitones up.
- **Reading the curve.** At filter level L, harmonic h reads the curve at its semitone position
  + (L − 96), clamped to 0–120, offset into the middle of the 241 points (points 59–180). So the
  table set for level L is the spectrum shaped by the curve, shifted by L − 96 semitones.
- **Cutoff to level.** The cutoff control spans 20 Hz–20.48 kHz on a log scale, ten octaves
  centred on 640 Hz, and maps onto 120 levels: **one level per semitone, now confirmed**.
  Key tracking adds (note − 60) × Key-Track; envelope depth adds up to ±48 levels. The result
  is smoothed with a one-pole lag.
- **Confirmed by measurement (8 November).** Nine probe presets were written with our own
  writer (`tools/make_probe.py`, `tools/probe_filter.py`) and rendered through the original in
  demo mode, noise left in.
  - **Probe design:** one oscillator, 64 equal harmonics, and a filter curve silent except one
    point.
  - **Settings covered:** cutoff on harmonics 2–32, 0 % and 100 % key tracking at C2, C3 and C4,
    and the curve point moved an octave.
  - **Result:** in every probe the original passes exactly the harmonic this model predicts.
    The interpolation leak into neighbouring harmonics matches too, to the decibel (h17 −38 dB;
    h31 and h33 −7 dB).
- **Sweep.** A filter sweep is a crossfade walk through these precomputed semitone levels.
  Fractional positions blend the two neighbouring levels. With the filter bypassed, only the
  unfiltered tables are read.

**Resonance [decomp].** The curve itself has no resonance.

- When the computed resonance mix gain exceeds its small threshold, each oscillator runs
  a two-pole state-variable band-pass
  on its output. Its tuning comes from a lookup table indexed by the cutoff position, and its
  damping falls as Resonance rises.
- The band-pass state passes through a soft saturation curve, so strong resonance stays bounded.
- Its output is added to the oscillator signal. The added gain grows with Resonance and is
  scaled by a cubic of the cutoff position, so resonance is strongest at low cutoffs.
- With unison spread active, the left and right channels each get their own resonator.

**Low-pass (LP Type, LP Offset) [decomp + decl].**

- **LP Type** chooses 1, 2, 3 or 4 cascaded one-pole low-pass stages ("LP+ 6", "LP+12",
  "LP+18", "LP+24" dB per octave; default LP+18).
- **LP Offset** (0–60 semitones) places its corner that far above the spectral filter's cutoff
  position, so it tracks the cutoff, key tracking and modulation.
- The top value, shown as "oo", switches it off. The coefficient comes from a 2048-entry
  lookup over the position.
- **Use [inferred].** It is a classic low-pass slope added on top of the drawn curve, useful
  for taming the top of bright spectra.

**Shape editing [sym/tbl].**

- Curves can be user-drawn and saved as `.slf`.
- `WRSCodeUtility_c::initFil{LP6,LP12,LP24,HP6,HP12,HP24,BP12,BP24,BP48,BR12,BR24,CM12,CM24,Flat}`
  give classic response shapes as starting curves.
- Filter Processing menu: Init to Flat, Normalize, Left↔Right, Top↔Bottom, Smoothen Shape,
  Save Filtershape.

**Per filter [host].** On/Bypass, Frequency, Resonance, Key-Track, Env-Depth (its own "Cut n"
envelope), Velocity, X-Mod 1/2, Volume, Pan, LP Offset, LP Type and Route Balance. The four
filters pair with the four oscillators.

## Precision ("Exactness") [decomp]

Precision is not a quality setting. It is **per-note random drift**.

- **When drawn.** At each new note, every oscillator draws five fresh random offsets.
- **Size.** Each offset scales with 4 × (1 − Precision)^1.5. At 100 % (the default) there is no
  drift. At 90 %, the lowest setting, the scale is about 0.13.
- **The five offsets**, with their relative weights:
  - pitch (× 0.0125);
  - filter cutoff (× 0.175, in the 0–1 cutoff range of ten octaves; about ±⅓ octave at most);
  - resonance (× 0.4);
  - amplitude (a multiplier 1 ± 1.25 × scale; about ±1.4 dB at most);
  - pan (× 0.625).
- **Unconfirmed.** The pitch offset's unit is not traced; it is added to the pitch stream
  alongside glide and bend. The other four were matched to their parameters through the
  modulation-slot map.
- **Use [inferred].** A built-in "analogue" humaniser, per note and per oscillator.

## Cross-modulation, measured (8 November)

Probes were rendered through the original in demo mode. Oscillator 1 and oscillator 2 are pure
sines, with the modulator 2 or 3 octaves above the carrier. The scripts are
`tools/probe_xmod.py` and `tools/probe_xmod_analyse.py`.

- **Oscillator slots:** types AM, FM and PM; sources OSC 1–4 (oscillator output) and FIL 1–4
  (filter output).
  - **AM** crossfades from dry to ring modulation: output = carrier × (1 − a + a × modulator).
    The predicted sideband ratios of 0.125 and 0.5 at 20 % and 50 % measured 0.112 and 0.477.
    At 100 % the carrier vanishes.
  - **FM** is linear frequency modulation scaled by the carrier's pitch. The swing is about
    12.1 × the carrier frequency at 100 %; it measured 2.42 × and 2.32 × at 20 %, at C3 and
    C4. The index halves when the modulator's pitch doubles. Deep settings run through zero.
  - **PM** is phase modulation of 0.486 rad per 1 % of amount, independent of the modulator's
    pitch. At 5 % the carrier nearly vanishes (index 2.44).
- **Filter slots:**
  - **FM** moves the filter position by up to ±48 levels (semitones) at 100 %, measured about
    0.47 per 1 %. This is the unidentified "×48" term in the filter-position formula.
  - **RM** showed no measurable effect in our probe. The newly available manual identifies it
    as resonance modulation (p25). Repeat with nonzero base resonance before inferring its law.
- **The OSC modulation source is taken after the modulator's amp envelope and before its
  volume.**
  - When the modulator's envelope decays to zero, the modulation fades; a −36 dB residue
    remains.
  - When the modulator's output volume is off, the modulation stays at full strength.
- **Reimplementation check.** partials reproduces these probes: AM and FM first sidebands
  mostly within 0.3 dB, PM within about 1.3 dB, filter-FM swing within 1 semitone. The original
  shows a faint second AM sideband (−25 to −30 dB) that partials does not have.

## Matrix scaling, measured (8 November)

**9 November correction from named-preset tests:** the main envelope is not part of
the cross-modulation source. At 25% main-envelope sustain, both self-PM and external PM
retain the same spectral ratios and fall by approximately 12 dB. Reducing the modulator's
own amp envelope does reduce modulation depth. The original partials implementation
incorrectly multiplied both envelopes into its source taps. The controlled references
are `renders/probe_character/feedback_*` manifests and `self_pm*`/`cross_pm*` WAVs.
Reference audio and demo noise are unmodified. Deep self-PM still shows a high-frequency
residual that is being isolated separately; matching a few low harmonics does not establish
equivalent feedback behaviour.

The corpus destination is `Filter n $Env-Depth` (hyphenated), including velocity routes
in Filth Bass and Tortured Lead 1/2. The importer previously accepted only the spelling
`Env Depth`; the core already supports this destination. Matrix depth targets also retain
their original 1..24 slot numbers across inactive/unsupported rows.

**OSC/FIL taps, measured 9 November:** `probe_character.py tap3` uses an audible direct
control to establish a 20 dB central filter dip and a fully rejected sine. OSC-sourced PM
is unchanged by either filter, while FIL-sourced PM follows the attenuation (and disappears
when rejected). Separate oscillator-volume probes establish that FIL includes oscillator
volume but precedes filter volume; OSC excludes both. Partials now reads the unfiltered
table for used OSC taps and applies oscillator gain to FIL taps. Unused OSC taps incur no
extra table reads. The previous implementation took both signals after the spectral curve.
The flat-curve attempts in `taps`/`taps2` do not prove spectral attenuation: the original
normalised those curves, including a peak outside the central window. Use `tap3` for this
claim. Its direct source controls agree within 0.1 dB, and carrier sidebands agree within
0.1 dB; the near-null carrier at 5% PM retains the previously known roughly 1 dB difference.

**Remaining self-PM discrepancy:** at 5% self-PM a 261.6 Hz sine has similar low harmonics
but much greater near-Nyquist energy in partials. Delays of 2..16 samples and simple feedback
averaging do not consistently reproduce the reference. Independent 2x synthesis plus a
low-pass before decimation is promising (at 10% PM, centroid about 3.34 kHz versus reference
3.31 kHz; at 100%, 8.81 versus 8.62 kHz), but does not yet explain all levels/depths.
These are hypothesis fits, not evidence that the original necessarily uses this design.
`model_self_pm.py` and `analyse_feedback_shape.py` contain our own scalar simulations.
Next: independent note/sample-rate checks and scalar DC measurements, then establish the
bandwidth and gain law before any oversampling change. No such change is in the engine.

LFO 1 (a 2 Hz sine) was routed to one destination at a time through matrix slot 1. The probes
were rendered through the original in demo mode; the scripts are `tools/probe_lfo.py` and
`tools/probe_lfo_analyse.py`.

- **Osc pitch:** ±48 semitones at 100 % (measured ±4.78 at 10 % and ±23.8 at 50 %).
- **Filter frequency:** the amount adds to the 0–1 cutoff, which spans 120 semitones. So 20 %
  swings ±24 semitones.
- **Osc wave mix:** the amount adds to the mix on its −1…+1 scale. So 50 % swings the B share
  0.25–0.75 around 0.5.
- **Filter pan:** the amount adds to pan. The pan law is linear: left ∝ 1 − p, right ∝ 1 + p.
  50 % gives ±9.4 dB between the channels.
- **Main, osc and filter volume:** the same law for all three. The gain falls as roughly
  (1 + amount × source)^0.55: 0.94, 0.85, 0.68 and about 0 at 10 %, 25 %, 50 % and 100 %.
  - The upward side is flattened by the original's output limiter in these full-scale probes,
    so it is not measured.
- **LFO rate:** matches its Hz setting.
- **Reimplementation check.** partials reproduces pitch, wave mix, pan and volume swing within
  0.1 semitone, 0.01 and 0.1 dB respectively.
- **Not yet measured:** symmetry, resonance, X-Mod amount, envelope-time and effect
  destinations, LFO speed, matrix depth, and the Note source's scale.

## Unison, measured (8 November)

Oscillator 1 was a pure sine, with voices and Spread Detune, Spread Volume and Stereo varied.
The probes were rendered through the original in demo mode; the scripts are
`tools/probe_unison.py` and `tools/probe_unison_analyse.py`.

- **Detune 0 means one voice.** The level is identical at 2, 3 and 6 voices, so the spread
  voices sound only when Spread Detune is above 0.
- **Positions.** The outer voices sit at ±Spread Detune; at 40 cents they measured −41 and
  +37.5 to +40.
  - 4 voices: ±1, ±0.5.
  - 5 voices: ±1, ±0.5, 0.
  - 6 voices: ±1, ±0.75, ±0.5, with no centre voice.
  - "4a" and "4b" measured almost the same.
- **Levels.** Voices add without normalising; Spread Volume (sv) crossfades.
  - The main voice is at 1 − sv/2: the centre one for odd counts, the top one for even counts.
  - Each other voice is at sv/2.
  - So at sv 100 % all voices are at 0.5, and at 20 % the main voice is at 0.9 and the others at
    0.1 (measured −19.5 dB relative).
- **Stereo.** A linear pan by position × Osc Stereo, with the upper-detuned voices going left.
  A centred voice sits at half level per channel.
- **Output level.** For the same settings the original is 5.7 dB above the uncalibrated
  partials output; partials now applies that as a calibration.
- **Reimplementation check.** partials matches all 11 probes within 0.6 dB, and the voice
  pitches within about 2 cents.

## Envelope shapes, resonance and limiter, measured (8 November)

Probes were rendered through the original in demo mode; the scripts are `tools/probe_env.py`,
`tools/probe_res.py` and their `_analyse` scripts.

- **Segment curves.** Curve 1.0 is linear in amplitude. Other values follow
  y = (e^(kx) − 1) / (e^k − 1) with k ≈ 8.25 × (curve − 1).
  - 0.2 is fast: the attack reaches 0.44 at a tenth of its time.
  - 1.8 is slow: it reaches 0.036 at half its time.
  - Decay and release mirror the attack (1 − shape).
- **Fade** is a slope while the key is held, about Fade dB per second: +1 rises and −1 falls,
  capped at full level. This is inferred from two probes.
- **Resonance** adds the band-pass output to the curve-filtered signal.
  - Boost at the cutoff harmonic: +6, +13, +21 and +24 dB at 25 %, 50 %, 75 % and 100 %
    (cutoff on h8). Lower cutoffs give more at low settings (+8.5 dB at 25 % on h2), and both
    saturate near +24 dB.
  - The skirts are broad: ±1 harmonic gets about +10 dB at high settings, and ±1 octave gets
    +4 to +6 dB.
  - partials models it with the documented gain law at half scale on the unnormalised band
    output, Q 2.5 to 20, and a soft saturation. The centre matches within about 2 dB; the
    skirts are still narrower.
- **Limiter.** The output never exceeds about +6 dBFS, however hard it is driven: +12 dB more
  drive moved the peak from 5.6 to 6.0 dBFS. partials uses a peak limiter at 2.0 with an
  instant attack.

## Envelopes, LFOs and modulation

- **12 envelopes**, all `EnvAHDSFR_c` (`ME05`). **[decl/host]**
  - Assignment: Amp 1–4 (per oscillator), Cut 1–4 (per filter), Mod 1–3, and Main.
  - Stages: Attack, Hold, Decay, Sustain, Fade, Release. The Fade stage keeps the level moving
    while a key is held.
  - Per-stage curve (0.2–1.8), pitch scaling and velocity scaling.
  - Tempo sync per stage, in sixteenths; an AHD mode; and "Ignore MIDI Note Off".
- **5 LFOs** (`LFO5_c`, `ML05`).
  - Waves: Sine, Triangle, Sawtooth, Pulse, Impuls, Peak, Noise, Sample&Hold, RandomPulse.
  - Rate in Hz, or synced from 16/1 down to 1/64, with dotted, triplet and odd values.
  - Delay and Attack; Symmetry; Phase.
  - Polarity: NORMAL, UNI +, UNI −, CUT +, CUT −.
  - Smooth; and Free-run modes Phase / Free / Pha/M (the global "LFO Mode" per LFO).
  - `SpectralTone_c::cloneGlobalLFOs` suggests each LFO can run per voice or globally.
    **[sym, inferred]**
- **Matrix:** 24 slots, each with a source, a destination and an amount (`PMX0`). **[host/tbl]**
  - **Sources:** Note, Velocity, Mod Wheel (plus a "Sub" variant), Aftertouch (plus Sub),
    Pitch Wheel (±), Arp Velocity / Transpose / Length, LFO 1–5, the 12 envelopes, Glide Curve,
    Random Bipolar, Random Unipolar, Alternate, Constant, MIDI CC 16–19.
  - **Destinations:** oscillator, filter and main parameters, envelope times, LFO speeds, other
    matrix depths, and **effect parameters** (`SpectralMatrix_c::getFxModIdx` /
    `parseFxTarget`).
- **Glide** (`RampGlide_c`, `MRG0`). **[decl]**
  - Mode OFF / HELD / ON / BEND, with Bend ±48 semitones.
  - Time in seconds, or synced; constant time or constant rate; curve.
  - Routed "to Pitch" and "to Cutoff", per oscillator and filter.
  - It also acts as a modulation source, with Note, Velocity, Alternate and Random source
    options. **[host]**
- **Chord memory** (`CMM1`).

## Arpeggiator and step sequencer (`StepSeq2_c`, `SEQ2`) [decl + decomp + data]

- **Settings.** Mode (Off, Mod-Only, Up, Down, Up/Down, Up/Down+Down/Up, Random, Chord, As
  Played), Resolution (1/1 … 1/64 with dotted and triplet values), Length (2–32 steps), Octaves
  (1–4), Swing, Velo Blend (played versus step velocity) and Note Length.
- **Step record.** Each of the 32 steps is a seven-character string of offset bytes (no byte is
  zero). The step code reads them as follows:
  - byte 1: the current step (rewritten on every change; not step data);
  - byte 2: the pattern length (also rewritten; a copy);
  - byte 3: **transpose**, in semitones (byte − 61; seen −12 … +24);
  - byte 4: **velocity**, 0–128 (byte − 1). 0 is a rest, and 128 holds the previous note
    without a new attack;
  - byte 5: **length**, 0–100 % (byte − 1). It is also the "Arp Length" matrix source;
  - byte 6: **note advance** (byte − 1). 0 follows the arp mode; n > 0 moves up n held notes,
    wrapping through the octave range;
  - byte 7: **legato** flag (byte ≠ 1). The previous note is not released, and the new one is
    passed to the voice as legato.
- **Matrix sources.** Velocity ÷ 127, transpose ÷ 48 and length ÷ 100 drive the matrix sources
  Arp Velocity, Arp Transpose and Arp Length. That is how "Mod-Only" lets the sequencer
  modulate without playing notes.
- **Corpus.** 84,736 steps decode cleanly, and 1,293 of them are legato. **[data]**

### Arpeggiator, measured (8 November)

A C major triad was held on a pure sine, at 1/16 and 120 bpm. The scripts are
`tools/probe_arp.py` and `tools/probe_arp_analyse.py`.

- **Gate.** Step length × Note Length × 2 of the step, capped at the full step. 50/50 gave
  64–66 ms of a 125 ms step, 25/50 gave 34 ms, and 100/50 and 50/100 were legato.
- **Swing.** Even steps are lengthened and odd steps shortened by swing / 2 of a step. At 50 %,
  steps run 156 ms and 94 ms, with onsets at 0, 0.156, 0.250 and 0.406 s.
- **Order.** Every mode starts on the first note of its list (the lowest), then moves:
  - **Down:** 48 55 52 48…, wrapping to the top.
  - **Up/Down and Down/Up:** both 48 52 55 52 48…; Down/Up bounces straight off the bottom.
  - **Up/Down+:** 48 52 55 55 52 48 48…
  - **Down/Up+:** 48 48 52 55 55 52 48…
- **Reimplementation check.** partials matches every probe: the onsets to the millisecond, the
  gates within 2–3 ms, and the order of all five bounce modes over 12 steps.

## Effects [decomp, behaviour only]

- **Chain.** 6 slots, each with On, Type and Route (a later slot or OUT). Every type exists in
  every slot (the host lists six instances of each), and Type picks which one runs.
- **Smoothing.** Each effect smooths its Wet and parameter changes with a short one-pole lag.

| Effect (class, tag) | How it works |
|---|---|
| Chorus (`Chorus6_c`, VCH5) | Modulated delay taps driven by two triangle LFOs at slightly different rates (spread), with interpolated reads and smoothed time changes; stereo spread, invert and pan |
| Phaser (`Phaser6_c`, VPH5) | 1–24 identical first-order all-pass stages per channel. A triangle LFO sweeps them exponentially around Center by Depth; the right channel's LFO is offset by Stereo. Feedback with optional inversion |
| Flanger (`Flanger6_c`, VFL5) | A short delay swept by a triangle LFO, the two channels offset by Stereo; feedback with inversion; the LFO can also move the pan (Pan Depth) |
| Delay (Mod) (`DelayMd6_c`, VDM5) | A delay line with an interpolated, smoothed read and feedback through low and high cut filters. A triangle LFO modulates the time (Mod Rate / Depth); stereo offset |
| Stereo Delay (`DelaySt6_c`, VDS5) | Separate left and right times, with feedback linking, cross-delay and filters optionally inside the feedback loop |
| Ping Pong (`DelayPP6_c`, VDP5) | A centre (pre) delay feeding left and right delays with their own pans; a balance between pre and L/R; feedback |
| Filter (`GloFil6_c`, VGF5) | A resonant two-pole multimode filter (LP/HP/BP/BR 12 dB) with input saturation (a soft clipper), an exponential cutoff, and a triangle LFO on the cutoff; gentle cubic limiting inside |
| Reverb (`Reverb6_c`, VR05) | Per channel: 8 parallel feedback combs with damping (a low-pass in the loop), whose lengths can be modulated by a few samples (Modulation, Mod Depth); then 2–6 all-pass diffusers in series (Diffusors). Reverb Time sets comb feedback. Pre-delay up to 300 ms, syncable. Line lengths are scaled from 44.1 kHz reference lengths |
| Param EQ (`EQ6_c`, VEQ5) | Two peaking bands (Freq, Gain ±18 dB, Q) plus low and high shelves |
| Compressor (`Compressor6_c`, VCP5) | Level detection in dB, a static threshold and ratio curve, and attack and release smoothing of the gain; output volume. The knobs use cube-root tapers |
| Wah-Wah (`WahWah6_c`, VWW5) | An envelope follower (attack, release, sensitivity) drives the cutoff of a resonant filter on the same ten-octave log scale as the oscillator filters; mild cubic saturation |
| Gator (`Gator6_c`, VGT5) | Two 16-step on/off patterns (left and right) at a synced step length. Smooth sets the gate edge time. Link: off, link, or cross-link the channels |
| Crusher (`Crush6_c`, VCX5) | Bit reduction from 1.00 to 16.01 bits, with optional dither; sample-rate division by 1–32 samples, in three modes (average, step/hold, slide); a resonant LP/HP/BP/BR filter before or after the divider; auto volume compensation |

- **After the slots.** The Master EQ (`EQ4_c`, VEQ4) has a low cut and a high cut, each with
  1–8 stages, plus 2 peaking bands. Its render function has variants per combination of
  active sections. A **Limiter** follows: a peak follower with attack and release smoothing of
  the output gain.

## Global and engine

- **Voices.** Mono, or 1–32 voices. **[tbl/host]**
- **Precision / Exactness.** Per-note drift; see above.
- **Preset data.** Presets carry the full spectra and filter curves in each `SAW1` section, so
  once saved they do not depend on the 101 built-in spectra. **[data]**
- **Migration.** A DSP version is stored (1.23 in current presets) and upgraded in steps
  (`updateToDSP0951/0970/1011/1070`).
- **Demo mode.** `GenDemoNoise_c` adds noise when unlicensed (see `reference_host.md`).

## Preset data model (complete)

- **Coverage.** `linplug_fxp.py fields` names **99.96 %** of all 2.47 million values in the
  2,648-preset corpus, with every section type covered, legacy ones included.
- **Unnamed.** One later-version field each in `VCH5` (it looks like a 0–100 % value) and in
  `SYG3`.
- **Spectra (`SAW1`).** Fields: Version, Filter, Wave A, Wave B, two reserved slots and
  **Wave Mix**.
  - The three "level strings" are the filter curve (241 points, level only) and Waves A and B
    (257 entries each, level plus phase).
  - The strings are stored as lists of byte values. No byte is zero, so every field is offset
    by one.
  - **Level:** two bytes, coarse and fine. dB = ((coarse − 195) × 250 + (fine − 1)) / 500. A
    coarse value of 1 or less means silent; the floor is about −97 dB.
  - **Phase:** one byte per harmonic; phase = (byte − 1) × 0.004 of a cycle.
  - **Reserved slots:** not declared, and 0 in all 10,592 sections.
  - **Wave Mix:** 0–100 %, default 50; seen 0, 50, 100 and many in between.
  - **Correction (8 November).** An earlier version of this document took the first reserved
    slot for Wave Mix and called the field unused.
- **Oscillators (`GOS0`).** Version and 4 reserved fields, then 38 parameters × 4 oscillators,
  stored **grouped by parameter**: all four Osc Cent values, then all four Osc Semitone values,
  and so on.
- **Global (`SYG3`).**
  - **Main settings:** Main Vol, Voices (mono, 1–32), Exactness (Precision).
  - **Reserved:** four undeclared slots, 0 in all presets.
  - **Velocity:** to volume, and to each filter's cutoff.
  - **Versions and modes:** DSP Version, and LFO Mode for each of the 5 LFOs.
  - **Effect slots:** 6 slots × (Type, On, retired Wet, Route).
  - **Routing:** glide and pitch bend to each oscillator and filter, bend ranges, and the glide
    modulation sources.
  - **Other:** Master EQ on, Osc FM HP Mode, and Sound Bank / Sound Id (licensing).
- **Matrix (`PMX0`).** The tools-menu state, then 24 sources, 24 amounts (raw ÷ 100 = %) and
  24 destinations, all stored as display names.
- **Arpeggiator (`SEQ2`).** Header settings, then 32 step strings (decoded above), then the
  tail settings.
- **Legacy effect sections.** The 14 old tags are earlier versions of the current effects.
  - `Spectral_c::getCompatibleChunkId` maps each old tag to the current one. **[asm]**
  - The mapping: VCH0, VCH2, VCH3 → VCH5; VCX1 → VCX5; VDM3 → VDM5; VDP1 → VDP5;
    VDS1 → VDS5; VEQ3 → VEQ5; VFL1 → VFL5; VGF1 → VGF5; VGT1 → VGT5; VPH0 → VPH5;
    VR01 → VR05; VWW1 → VWW5.
  - Every old layout is the start of the current layout, so the current module loads it as an
    older version. **[data]**
  - These are exactly the effect versions CronoX saves (see `linplug_findings.md`).
- **Display scaling.**
  - **General rule:** a stored integer ÷ 10^decimals.
  - **Exception:** percent parameters with 2 declared decimals and a range within ±100 show the
    raw value.
  - **Checked against:** 18 values in Spectral's own parameter display.
- **Round trip (8 November).** Re-encoding every parsed preset gives back the original unpacked
  chunk **byte for byte, for all 2,648 presets**, which proves the token model loses nothing.
  - Our own run-length packing expands back to the same data. It is within 0.11 % of
    Spectral's packed size.
  - It does not reproduce Spectral's own packing choices, which was not attempted. **[data]**

## Still open

1. **What the third table bank per oscillator holds.** It is possibly a mix or display buffer.
2. **The pitch drift's unit under Precision.**
3. **The exact reverb line lengths and the chorus and delay modulation depths** in physical
   units. These are better measured from renders than read from code, which keeps the
   clean-room boundary, but renders need a licensed copy (see `spectral_rebuild_plan.md`).

Decompiler output stays in the session scratchpad and is not committed. It is analysis of
third-party code and is used only to describe behaviour.

## Effects, output stage and volume, measured (8–9 November)

Probes: `tools/probe_fx.py` (round 1, `renders/probe_fx/`) and `tools/probe_fx.py --round2`
(`renders/probe_fx2/`), rendered with `tools/probe_render.sh DIR`, which now takes per-probe
note start, length and duration from the manifest. Analysis: `tools/probe_fx_analyse.py`
(`PROBE_DIR=probe_fx2` selects round 2). Levels are read at the played harmonics; the demo
noise is never removed. "in" below is the level at the effect input in dB relative to a
full-scale sine, from the slot-off references.

- **Effect sections and slots.** The first token of an effect section is its slot index,
  0-based. All 2,645 corpus presets store their six effect sections in slot order, so mapping
  sections to slots by position (as the importer does) is correct. A probe that moves a
  section into another slot must rewrite that token; round 1 did not, which is why its
  compressor and crusher probes showed no effect.
- **Main Vol. is applied after the effects.** Changing Main Vol moves a compressor's output by
  exactly the same dB. partials applied it per voice, before the effects, and must move it.
- **Osc Volume range** is −12 to +12 dB: −24 and −18 played at the −12 level, and no corpus
  preset goes below −12.
- **Compressor** (`VCP5`).
  - Make-up gain is automatic and full: −Threshold × (1 − 1/Ratio). For example, +4.46 dB
    measured where +4.5 is predicted (−6 dB at 4:1, below the threshold).
  - Hard knee. Above the threshold, out = in/Ratio + C. C is 1.84 dB at 4:1, 1.22 dB at 2:1
    and 2.38 dB at 40:1, the same for every threshold. This fits a detector that reads a
    steady sine 2.44 dB under its peak. A linear peak follower with 1 ms attack and 100 ms
    release reads −0.3 dB, RMS reads −3.0 dB, so the detector law is still open.
  - Out Vol. is in 0.2 dB steps (±6 checked). Attack and Release hold milliseconds (presets:
    50.0, 500), despite the "s" in the declarations.
  - Ratio 1 leaves the level unchanged.
- **Output stage** (no effects, sine, Main Vol 0). Clean up to about 0 dBFS. Above that it
  compresses smoothly towards about +6 dBFS, with only mild distortion:

  | in (dB) | out h1 (dB) | out peak (dBFS) | h3 re h1 (dB) |
  |---|---|---|---|
  | −0.5 | −0.5 | 0.1 | noise |
  | 2.5 | 2.1 | 2.4 | −38 |
  | 5.5 | 3.6 | 3.7 | −34 |
  | 8.5 | 4.5 | 4.5 | −33 |
  | 11.5 | 5.1 | 5.1 | −33 |
  | 14.5 | 5.5 | 5.4 | −33 |
  | 17.5 | 5.8 | 5.7 | −33 |

  A static shaper alone gives far too much h3 (−22 dB at the top). A fit with an
  envelope-driven gain stage followed by a soft shaper y = 1 + o/(1 + b·o) (o = |x| − 1,
  above 1) needs b of about 0.15–0.3 and gain reductions of −1.6, −3.6, −6.0, −8.6 and
  −11.3 dB at 5.5–17.5 dB in. Attack and release times are not measured. partials still
  uses a hard peak limiter at 2.0.
- **Filter effect** (`VGF5`).
  - Level (Wet only) is in 0.2 dB steps: −6, 0 and +6 dB measured exactly.
  - Saturation lowers the level and tends towards a square wave at 100 %. Its distortion is
    the same at −12 and 0 dB in (h3 re h1: −37 dB at 20 %, −21 dB at 50 %, −10 dB at 100 %),
    so the shaper looks homogeneous, like y = c·sign(x)·|x|^p. Round 1 fits: p ≈ 0.93, 0.63
    and 0 at 20, 50 and 100 %. Round 2 adds 10, 35 and 75 % and a Main Vol pair, not yet
    analysed. partials' current saturation boosts by up to +11 dB and is wrong.
  - The cutoff LFO and filter-type probes of round 1 drove the output stage, so they are not
    usable. Round 2 repeats them at −48 dB, not yet analysed.
- **Gator** (`VGT5`).
  - The step clock follows the host transport, not the note: notes starting at 0.3 s and
    1.07 s both had step 1 open at the 2.0 s bar line (120 BPM, 1/16).
  - Link copies left to right; Cross-link makes right the inverse of left. Both confirmed.
  - Wet 50 %: open steps at 1.00, closed steps at 0.25, which fits (1 − wet)². Round 2 adds
    25 %, 75 % and the older Unused Wet field set to 0 and 200, not yet analysed.
  - Smooth acts like a one-pole on the gate: time constant about 2 ms at 0.25, 4 ms at 0.5
    and 132 ms at 1.0 (at 1.0 the gate never fully opens on 1/16 steps). Round 2 adds 0.05,
    0.1, 0.75, 0.9 and 1/32 and 1/8 steps, not yet analysed.
- **Wet field migration** (corpus, not a probe). Version-5 effect sections write an older
  0..200 % wet field and a newer 0..100 % one. Re-saved presets show new = old / 2 at
  conversion, so a section without the newer field reads the older one divided by 200. The
  importer now does this for every effect except chorus.
- **Crusher** (`VCX5`): round 2 probes (bits 2, 4, 8; auto volume on and off; inputs −12, 0,
  +12; divider modes; divider 2.5) are rendered but not analysed. Older saves keep the divider
  in the field named "Unused" (equal to Divider where both exist).
- **Wah** (`VWW5`): not probed. Level (D+W) holds a linear percentage (presets: 100, 41, 492),
  and Sensitivity is signed (−0.03 .. 0.03 displayed). Both readings are inferred.

### Round 2 analysis, 9 November (Codex)

`PROBE_DIR=probe_fx2 python3 tools/probe_fx_analyse.py all` now measures all five sets;
`model` fits original behavioural models. The WAV reader validates RIFF chunks and format.
Harmonic levels use a Hann-weighted projection at the exact MIDI-48 frequency, not the
largest bin within ±2% (which can select noise or a neighbouring harmonic). This is scalar
analysis of untouched recordings, with no modified audio output. Reports before DSP edits
are `renders/probe_fx2/analysis_codex_coherent_before.txt` and `model_codex.txt`.

- **Filter saturation.** The 12 dB oscillator-volume pairs give the following approximate
  homogeneous transfer `c * sign(x) * abs(x)^p`, calibrated at effect input 5.7 dB. This is
  our fitted model, not recovered source. Interpolate the small measured parameter set;
  0% is identity. The inferred powers from h3 agree within about 0.02 (except the smallest
  noisy harmonics). Exactly zero input must remain zero.

  | Sat % | p from input-level pair | c | h3/h1 at higher input (dB) |
  |---|---|---|---|
  | 10 | .98061 | .842542 | −44.59 |
  | 20 | .94006 | .714045 | −36.09 |
  | 35 | .82821 | .565208 | −27.37 |
  | 50 | .63834 | .464006 | −20.62 |
  | 75 | .22255 | .373560 | −12.58 |
  | 100 | .00175 | .347807 | −9.53 |

  At 50%, Main Vol −24/−12/0 gives h1 −34.09/−22.04/−10.00 dB. This independently confirms
  that Main Vol is after saturation. The fitted model is only established for sine probes.
- **Quiet rich-spectrum probes are inconclusive.** `FIL_rich_ref` has only 1–9 dB harmonic
  prominence above the neighbouring noise floor in the analysis window. The filter-type,
  LFO and crusher-divider results cannot establish their transfer laws. Earlier peak-bin
  comparisons misleadingly gave almost exactly 0 dB differences because they selected the
  same noise peaks. Do not tune those mechanisms from these recordings. Re-probe with
  fewer/louder harmonics and first verify the slot-off level and preset field encoding.
- **Crusher bits/auto volume.** These louder sine probes are usable. At 2 bits, auto off,
  oscillator −12/0/+12 gives h1 −19.48/−8.36/+3.51 dB; auto on gives −17.99/−5.94/+3.95.
  Auto on keeps h3/h1 near −39 dB and h5/h1 near −24 dB at the two lower levels, suggesting
  level-dependent normalisation, not a fixed output trim. At 8 bits auto is almost neutral.
  A simple nearest quantiser does not explain the low-input square wave; a mid-rise
  quantiser can fit one level but not the others. Its precise quantisation/auto law is still
  unresolved. Correcting input gain placement is required before comparing our version.
- **Gator wet.** 25/50/75% agrees with closed-step gains .5625/.25/.0625. Changing the retired
  wet field from 0 to 200 does not change a newer section's output. Timing uses transport.
  A periodic one-pole fit gives roughly 1 ms at .05/.1, 4–5 ms at .5, 12–16 ms at .75 and
  32–41 ms at .9. The .5 time is similar at 1/32 and 1/8, so it is not proportional to step
  length. At 1.0 a single causal pole does **not** fit both rates; residuals/phase disagreement
  remain large. Use an explicitly provisional time curve, not a claimed exact law.
- **Output stage correction to the earlier table.** Coherent h1 at oscillator −3/0/3/6/9/12,
  Main Vol 0, is 2.27/3.79/4.72/5.32/5.73/6.00 dB. Corresponding h3/h1 is
  −35.18/−29.83/−27.92/−26.96/−26.41/−26.06 dB. Thus the earlier “near −33” summary is not
  accurate for the loudest round-2 probes. Peaks remain below 2.0. Our implementation will
  use a stereo peak envelope, a small measured steady-state gain curve, and a gentle shaper
  above 1.0 (`b=.3`), with an independent ±2 safety ceiling. Instant attack/100 ms release
  are engineering choices, not measured time constants. Static levels are testable; the
  model is not evidence of exact transient or crest-factor fidelity.

### Partials verification after these changes, 9 November

- **Core:** 267 checks pass normally and with ASan/UBSan, including measured compressor and
  saturation levels, output fundamentals/transient bounds, post-effect Main Vol, gator wet,
  negative PPQ, seeks, split calls and note-on independence.
- **Reference comparison:** all existing round-2 probes re-rendered on the partials side only.
  Compressor fundamentals are within 0.2 dB, saturation fundamentals within 0.05 dB, and
  output-stage fundamentals within 0.04 dB of the noisy originals in the analysis window.
  Saturation's higher harmonics remain approximate (especially weak harmonics near noise);
  the output stage's h3 is still about 8 dB too weak just above the knee, converging within
  0.5 dB at the loudest input. These are calibration comparisons, not independent unseen
  waveform validation.
- **Unison:** all eleven existing probes rerendered; levels remain within about 0.6 dB. The
  three zero-detune cases match to the displayed 0.1 dB. Moving calibration upstream did not
  disturb the linear dry level.
- **Preset A/B:** `YW 1 note love` has −0.5 dB harmonic-power difference; `bt bellish approach`
  about −2.5 dB. `edt Slider Lead` remains about +5.7 dB and bright; the crusher preset and
  other quiet patches remain imperfect. Broadband RMS is not a clean synth level because
  the reference contains demo noise. Full reports are under `partials/build/`:
  `effect_comparison.txt`, `unison_comparison.txt`, `preset_comparison.txt`.
- **No new original runs.** `probe_render.sh --partials-only` and
  `partials/tools/verify_offline.sh` reuse the saved reference WAVs without modifying them.

### Round 3: waveform reference correction (9 November, Codex)

`tools/probe_fx3.py` uses staged original/partials probes in `renders/probe_fx3/`. Every
original run uses the previously approved offline host under `no_network.sb`; WAVs retain
demo noise and are never overwritten. Single-tone references have about 59 dB fundamental
prominence. Direct output, bypassed effect slot and a wide-open enabled slot agree within
0.01 dB. The old low-level 64-harmonic reference has only 4 dB prominence.

The unexpectedly quiet rich reference exposed **missing waveform peak normalisation** in
partials, rather than a slot-routing problem. With equal-phase, equal-level harmonics,
original fundamental attenuation relative to a sine is 4.90 dB (two), 10.17 dB (four), and
about 21.5 dB (sixteen). These agree with scaling the summed waveform down to unit peak.
Cosine-phase pairs/fours give 6.01/12.02 dB; varied-phase spectra have different reductions,
confirming this is phase-dependent peak scaling, not harmonic count or RMS normalisation.

Separate A=sine/B=four-harmonic probes at 0/50/100% mix confirm independent A/B scaling.
Filtering away every harmonic except the fundamental keeps the same gain reduction; so does
playing sixteen-harmonic spectra at C3/C6/C8 with different band-limit tiers. Independent
saw-series probes show 3.98/5.11 dB attenuation for 5/31 partials. Square-series probes,
whose summed peak is below one, keep the sine's fundamental level: **normalise down only**.

The correction is in our table builder, entirely at load time: form the unfiltered cycle,
measure its peak, divide all harmonic amplitudes by `max(1, peak)`, then build the existing
filter levels/tiers. Eight targeted checks cover measured levels, sub-unity waves, filtering,
band limiting and silence. Earlier tier/filter tests now compare relative gain because a
unit amplitude for every harmonic was the disproven assumption. Core: 275 checks pass
normally and under ASan/UBSan. Plugin/realtime qualification follows after this pass.

### Round 3: filter and crusher results (9 November, Codex)

The `filters`, `crusher`, and `filter_check` stages use isolated sines at MIDI notes
48/60/72/84/96/108, with an explicit slot-off reference and documented oscillator/filter/Main
volumes in each manifest. This separates oscillator normalisation from effect behaviour.

- **Filter effective cutoff:** the displayed value divided by approximately pi. The HP/LP
  amplitude ratio independently locates the corner near 333 Hz when the control says
  1046.5 Hz. Follow-up settings 500/2000/8000 Hz support the same scaling. LP, HP and BP
  have equal gain at the effective corner; BP is not normalised to unity as resonance rises.
- **Filter resonance:** low-input centre probes (effect input about −18.35 dB) measure
  gains −0.10/4.68/10.03/13.59/14.46 dB at 0/25/50/75/90%. A trapezoidal SVF with damping
  1.012/.584/.315/.209/.189 models those sines. This is a measured approximation;
  the higher-input probes show different effective damping, and distortion/transient
  behaviour is not reproduced. The 100% endpoint .18 is extrapolated. The previous
  generic damping remains in crusher/wah; this evidence is for the filter effect only.
- **Filter LFO:** paired LP/HP measurements imply a triangle starting at zero and rising,
  with ±3 octaves at full depth. At 0.5 Hz/depth 50%, effective cutoff goes from about
  333 Hz to 930 Hz at 0.5 s, returns to 333 at 1 s and reaches about 120 Hz at 1.5 s.
  Reset now restores this phase. Song-position synchronisation of this LFO is still
  unmeasured; only its depth/rate/initial phase are established here.
- **Crusher AVG:** a continuous first-order smoother, `y += (x-y)/divider`, rather than
  a box average followed by sample-and-hold. Divider 2.5/4/8 probes at three notes match
  this response. STP and integer SLD retain their implementations; the fractional SLD
  result has a small residual discrepancy.
- **Crusher quantisation/auto volume:** still unresolved and unchanged. One bit produces
  a square at either tested input, while our nearest quantiser can silence low inputs.
  A mid-rise model with `q=(2**bits-1)/2` explains low-input levels but fails at higher
  input, so it was not implemented. Auto volume is level-dependent, not the provisional
  fixed trim in our implementation. Histograms of raw noisy samples did not establish
  reliable plateau positions; do not mistake that diagnostic for a transfer measurement.

After implementation, all 304 core checks pass normally and under ASan/UBSan. Original
WAVs were retained; only the partials sides were rerendered. Waveform probes with good
confidence are within 0.1 dB except the 64-harmonic case (0.26 dB). Crusher AVG h1 is
within 0.08 dB across nine probes. Filter LFO h1 is within 0.25 dB across six probes;
the extra static cutoff probes are within 0.6 dB. Higher-input resonance still differs by
about 1.1 dB in some cases. Centre-gain results are calibration, not independent validation.
Reports: `partials/build/fx3_{filters,crusher,filter_check}_verification.txt`;
JSON and before-change snapshots in `renders/probe_fx3/`.

**Full-preset caveat:** correcting normalisation does not uniformly improve every preset's
aggregate level; earlier errors partly compensated for others. The final nine-preset report
is `partials/build/preset_comparison_fx3_final.txt`. Slider Lead moves from about +5.7 dB
to −5.7 dB harmonic-power difference; Seeanemone is now −11.4 dB. Do not revert the
independently validated normalisation to tune these presets. Next isolate oscillator/filter
and effect contributions for those cases and establish why their gain is still wrong.

**Legacy check:** the probe base stores DSP Version 1.072, Slider Lead/Seeanemone store
0.974, and “bt 7th movements” stores 1.21. Six additional controlled probes changing only
that field to 0.974 or 1.21 produce the same sine/four-harmonic/filter results. Version
selection alone does not explain the remaining preset gain differences. This is evidence
for these mechanisms, not a claim that the version field never affects other behaviour.

**Local plugin qualification:** rebuilt AU/Standalone Release, both signatures verified.
Processor ABC stress with 20 presets passes in ASan/UBSan and Release, no bad samples or
sanitizer errors. Core 304/0. Heavy-burst real-time overruns remain, with high host-load
warnings during qualification; see `partials/docs/design.md` for timings and artifact logs.

### Manual-led measurements and implementation, 9 November (Codex)

Fresh references are in `renders/probe_manual/`, produced by `tools/probe_manual.py`.
The original audio retains demo noise. These findings supersede earlier provisional
matrix-depth, spread-source and native-rate feedback assumptions:

- **Matrix depth is additive:** Constant into row 7's depth makes base 0+5 equal direct 5,
  base 5+5 equal direct 10, base 5-10 equal direct -5, and base 30+80 saturate at direct 100.
  Partials now clamps the effective amount to [-1,1] and retains valid zero-base rows.
  The importer repair restored 2,386 missing targeted rows across 1,191 patches, with
  backups in `partials/build/matrix_zero_backup/`. This restores Tortured Lead 2's
  aftertouch-controlled vibrato. Depth-of-depth routing is still not recursively resolved.
- **OSC cross-modulation uses one full-level spread voice.** Three-voice sources use
  the centre voice: detune 0/40 and spread-volume 100/20 produce the same source sidebands.
  The tested two-voice case uses the upper detuned voice. Partials now selects that voice
  and ignores spread volume/stereo for the OSC tap. FIL taps still aggregate filtered
  stereo and need separate investigation. Previous measured amp/Main envelope behavior
  remains intact.
- **Phase zero aligns spread voices.** Three-voice phase 25/50/100 fits approximately
  .5/1/1.5 times the phase setting. Partials uses that symmetric phase law; extension
  to other voice counts is an approximation, not an independent measurement.
- **Self-PM needs a higher internal rate and output filtering.** Held-out note 72/48k
  and note 60/96k tests favor 2x over 1x or 4x synthesis. At PM10, original RMS/centroid
  are -29.82 dB/4103 Hz and -29.54/5990; the independent 2x model gives -29.77/4109
  and -29.52/6164. PM100 levels agree within .3 dB. The integrated implementation
  renders active cross-modulation patches at 2x and decimates each send before effects,
  using our generated 63-tap Hann-windowed sinc. Effects/arp/transport stay at host rate.
  This is an independently implemented behavioral model, not proof of the original's
  architecture. Causal filtering adds approximately 15 output samples of delay.
- **True-FM HP modes remain unresolved.** Raw Off/F100/D40 options differ strongly
  in self-FM 20 settling: Off stalls while the others sustain a pitched signal. Several
  low-corner HP models fit steady harmonics, so those fits cannot establish the actual
  transient/feedback law. Legacy HiHat resembles F100 in its early window, insufficient
  evidence for a universal legacy default. No HP-mode DSP was added.

The integrated 2x named-preset comparison (`partials/build/oversampling_named_verified.json`)
puts Tortured Lead 1's .11-.18s RMS/centroid at -6.2 dB/317 Hz versus -5.8/298 original,
and Lead 2 at -12.2/535 versus -12.4/558. HiHat is -28.9/8533 versus -30.3/10597.
These are fixed-note attack windows; late spectra still differ. Filth Bass remains too
loud and dark (-10.7/58 versus -14.5/94 in that window). Its isolated oscillator 2 is
within .3 dB; oscillator 1's resonance/modulation contains most of the remaining mismatch.

**LFO follow-up:** slow sine-to-pitch references establish UNI- as `(sine-1)/2` rather
than `-(sine+1)/2`. At exactly zero initial phase, both sine unipolar offsets start at
zero and ramp to their normal +/-0.5 over about 1/24 cycle. Rates .05/.5 Hz, matrix
amounts 10/20, later cycles, and initial phases 1/2/4/8/25/50/75% distinguish this
startup behavior from an altered steady waveform. Nonzero phase skips the ramp.
Partials models the offset slew independently, scoped to
sine/zero-phase/non-free-running startup. The original's internal interpolation algorithm
is not established. This matters
to Filth Bass's slow LFO1, which simultaneously drives PM depth and filter cutoff.
See `lfo_{origin,cycle,start}_fit.json` for scalar pitch fits through the noisy audio;
final app qualification and named-preset comparison are recorded in Partials' design log.
After a temporary host-load delay, the LFO change built and passed all 349 core checks
normally and under ASan/UBSan. Slow UNI+ peak pitch is 309.50 Hz against 309.42 original;
UNI- is 236.77 against 236.97. Filth's early level becomes -13.0 dB versus -14.46 original,
but its centroid overshoots to 162.5 Hz versus 94.3; its tone remains unresolved. The other
three named comparisons above are unchanged. Full comparison:
`partials/build/lfo_named_verified.json`. Final plugin qualification is recorded in
the Partials design log. No unbuilt proposal needs to be reapplied.

The final AU/Standalone Release and broad/named processor stress runs now pass in Release
and ASan/UBSan, with 349 core checks and four importer tests passing. No bad samples;
bundle signatures verify. CPU deadline overruns remain in stress tests, so this is not
a claim of guaranteed real-time performance or complete sound equivalence. Logs, hashes,
remaining sound priorities and timing limits are in `partials/docs/design.md`.

### Low-note PM, spread gain and symmetry order, 9 November continuation

Fifty additional offline references in `renders/probe_manual/` cover `low_pm` (26),
`spread_gain` (10), `spread_check` (4), `pm_symmetry` (8) and `pm_symmetry_check` (2).
Original audio/noise is unchanged. The PM implementation was not retuned globally.

- **Shared spread feedback remains supported.** At note 24, PM 4.6, three voices,
  detune 6.19 and stereo 83, reference side RMS is -34.43 dB; our independent shared-source
  model is -33.87, but separate per-voice feedback is -26.38. Single-voice low-note PM
  levels are already within about .3 dB, although brightness still differs at some depths.
- **Spread gains were slightly high.** New individual-voice fits supersede the old
  half-gain approximation. At full Spread Volume, each voice is approximately .467
  of the single oscillator; as Spread Volume `s` rises, the main voice crossfades
  from 1 to .467 and each other voice from 0 to .467 across the tested voice counts.
  Held-out 35% probes at 96k/note 48 and 48k/note 72 confirm additional gains .1635/.164
  and main gains .8097/.8118. The corrected engine gives .1634/.1635 and .8130/.8137.
  Two valid equal-final-level oscillator/Main pairs agree exactly. Earlier controls
  outside the established ranges did not preserve the intended levels and were excluded;
  the exact bounds and whether those inputs were clamped or rejected remain unverified.
- **PM precedes the waveform symmetry warp.** External PM 5 at 25/75% carrier symmetry
  gives reference RMS -16.07 dB and centroid 1806 Hz. Our previous order gives -15.66/1696;
  an independent PM-before-warp model gives -16.07/1796. Held-out PM 2, note 48, 96k,
  symmetry 35/65 gives reference -15.68/-15.69 and model -15.64/-15.65. Partials now
  adds/wraps the PM phase before the existing symmetry warp. Neutral 50% is unchanged.
  Self-PM levels improve in the low-note tests, but not every feedback spectrum matches.

Corrected Filth oscillator 1 is within .21 dB of the reference with its filter bypassed,
and also within .21 dB with resonance off. With resonance on it remains 1.52 dB loud.
The full preset still differs, particularly later in the held note; these fixes do not
close that complaint. Final named comparisons are in `partials/build/bass_named_verified.json`.
The 363-check core passes normally and under ASan/UBSan, including measured spread
amplitudes and PM/symmetry cases; plugin qualification is recorded in the design log.

### Saved-code reinspection and independent Claude review, 9 November

Yan challenged the claim that Codex could not see the original DSP, then explicitly asked
Claude to confirm the interpretation using its installed reverse-engineering skill.
The original source is unavailable, but the compiled instructions and Ghidra reconstruction
are inspectable. This was already the basis of the earlier `[decomp]` sections. The agreed
reimplementation boundary permits understanding behavior while forbidding translation or
copying of proprietary routines and large data tables.

The saved evidence is `GenSpectral_c::renderNX` at `0x000dede0`, in Claude session
`a270d5c1-3fd4-4b5e-a340-cc819f9e215c`'s `scratchpad/decomp/renderNX.c` under
`/private/tmp/claude-501/-Users-yanwhite-Documents-Music-docs-Code/`. Codex read the relevant
branches; Claude independently read all 954 lines, plus selected `spectral_core.c` sections.
Its [review](claude_filter_review_2026-11-09.md) records the exact scope. No new Ghidra run,
assembly validation, original-plugin execution or filter implementation change occurred.

**Supported by the decompiler reading:**

- At `renderNX.c:866-894`, the resonance band state is saturated before being stored and
  before driving the other integrated state. Partials currently keeps its two states
  linear and applies a much gentler soft curve only to the added output. Matching a few
  steady resonance boosts did not establish equivalent nonlinear behavior.
- The original's state update resembles an explicit state-variable filter; our implementation
  uses a trapezoidal form. The original's nominal damping can become negative at high
  resonance (`renderNX.c:433,863-864`), whereas ours stays positive. Actual self-oscillation,
  stability and level dependence still require the correct coefficient and drive mapping.
- With one voice, the resonator input is after the spectral curve but before LP+
  (`645,682-695,866-875`). With spread, its inputs are the stereo sums after LP+
  (`700-735,795-835,878-894`). Resonance is added after LP+ (`897-902`). Our current
  engine runs LP+ over the dry-plus-resonance sum. Do not treat the original as one
  universal serial filter chain.
- The output level/envelope product at `716,844-845` acts after LP+ and on the resonance
  output. Other gains already affect the resonator's input (`603,645,695`); their exact
  mapping to UI/matrix Osc Gain, Osc Volume and Fil Volume is unresolved. Claude corrected
  Codex's first revised diagram, which had placed all smoothed level factors before LP+.
- The phase offset is added and wrapped before symmetry distortion (`601,625-635`),
  corroborating the measured PM-order correction. The mapping from the modulation block's
  field to the UI PM control is partly inferred, supported by the controlled probes.

**2x is still an architecture inference:** the loop runs twice its length argument and
phase increment includes a half factor (`125,247,950`); setup scaling supplies further
clues (`spectral_core.c:10058-10064`). **Follow-up correction after Yan asked whether
Downsample_c is the bitcrusher:** the earlier review inferred a user effect from its
constructor location and parameter method. Direct inspection weakens that inference:
its parameter method declares only Version; the separate Crush6_c declares Resolution,
Divider, Divider Mode and Wet. Saved assembly for Downsample_c::render (0xdb3f0) reads
two successive float samples per channel per iteration, updates filter states twice,
and passes one result per channel to its output method. This supports an internal
2:1 rate-conversion role, not identification as the selectable Crusher. The generator
connection still needs tracing; its name or constructor location alone proves neither role.
The caller's length,
sample-rate fields and downstream decimator connection have not been traced. These are
strong static clues alongside our measurements, not proof of the whole processing graph.

**Reconciliation and next measurement:** Claude's proposed level sweep is useful, but
some of its wording is too broad. Lookup-table contents and caller connections are not
inherently inaccessible statically; they simply were not fully traced in this review.
An unchanged resonant/dry ratio would not rule out in-loop saturation if both tested
controls are post-resonator or the drive is too low. Also, Filth uses spread, whose
resonator input changes when LP+ changes; its resonant peak need not stay constant.

Next isolate Filth oscillator 1 with LP+ off, fixed note/velocity and modulation held
constant. Sweep Osc Volume and Fil Volume separately through valid 0/-6/-12/-18 dB
settings, comparing resonant-to-off-resonant harmonic levels in .2-.8s. If neither
control changes relative resonance, test a verified pre-resonator Osc Gain matrix route
or controlled spectral input scaling rather than declaring the feedback linear.
Then compare LP+ on/off separately with one voice and three detuned voices. Reference
WAVs/noise remain untouched. Only implement an independent nonlinear model after these
tests separate drive, topology and signal-order effects; retain the qualified 363-check
app in the meantime.

### Verified Mac and Windows comparison review, 9 November

Actual Claude reviewed the extracted Windows metadata and selected saved Mac code using
the installed reverse-engineering skill. Its [report and Codex reconciliation](claude_windows_mac_review_2026-11-09.md)
recommend retaining the verified Mac binary as the primary map, with Windows x64 then
x86 for targeted cross-checks. Native `nm` now confirms that the archive Mac VST retains
named `GenSpectral_c::renderNX`, `prepProc`, constructor and parameter methods. Windows
has class RTTI and x64 unwind metadata, but only one exported entry point and no included
PDB. No Windows DSP or new archive Mac DSP was decompiled in this review.

Next static action, before changing resonance: import the verified archive Mac executable
in an isolated Ghidra project; compare renderNX/prepProc and relevant setters against the
old Toshiba-derived analysis. Validate suspect state writes and single/spread branches
against assembly. Trace parameter setters and generator caller/output consumer to resolve
gain mapping and the actual rate/decimation path. Use Windows RTTI/call graphs for a specific
remaining ambiguity, not an assumed identical vtable order or a class-name-based filter model.
Retain the planned hash-labelled archive-reference audio probes to test actual drive and
held-out preset behavior. No implementation or qualified app change came from this review.

### Downsample versus Crusher clarification

The manual's Crusher combines bit-depth and sample-rate reduction (cached Manual.txt,
lines 1880-1941). Saved `spectral_more.c:14039-14080` maps those controls explicitly to
Crush6_c. Downsample_c::getParaSetPtr at 10126 declares only a Version field; a parameter
method by itself does not establish user-adjustable effect controls.

Codex read the saved assembly at spectral.s:219542-219820, including the full Downsample
constructor, parameter method, render and sample-rate setter. At 0xdb57e/0xdb59b its
input pointers advance eight bytes; the two float samples at offsets 0 and 4 each feed
state updates, followed by output-method calls at 0xdb661/0xdb675. This is stronger
rate-conversion evidence than the previous constructor-neighbor inference. No bit-depth
quantisation is visible in that routine. Still pending: output-method semantics and the
caller/buffer connection to GenSpectral, then validation against verified archive code.

Two attempted native objdump symbol-selection invocations on the verified archive failed
at argument/action handling; stop that invocation approach here. The successful assembly
inspection above used the existing Toshiba-derived listing only. Before retrying, check
the tool's required disassembly action and symbol-filter syntax, or use the already
documented isolated Ghidra workflow. No new binary was executed and no DSP was changed.

### Verified archive DSP, complete rate path and filter-drive measurements (9 November)

This section supersedes the earlier pending-caller statements. The archive Mac VST
executable SHA-256 is `4a9945bdcfcbb79b32c1d602f14c7ad2e26fdd79c611f6ff3d5700b0bc7d1eeb`.
`tools/compare_archive_dsp.py` found identical addresses, sizes and bytes in all **55
selected text symbol spans** for GenSpectral, SpectralTone, SpectralInst and Downsample
versus the old reference. These are spans bounded by adjacent text symbols, not a proof
that all dependencies or the whole program match. The hash-only report is
`corpus/internet-archive/spectral-1.2.7/dsp_symbol_comparison.json`.

Fresh Ghidra analysis of the verified archive x86_64 slice resolved the caller and buffer
path. Decompiled routines remain in `/private/tmp/partials-archive-dsp-01a1bff2/`; only
observations, addresses and our analysis tools belong in the repository.

1. `SpectralTone_c::render` (0xa8340) calls `Generator_c::render` (0x3d330), then
   `renderOne` (0x3d3a0) and `GenSpectral_c::renderNX` (0xdede0), preserving the host
   frame count. The generator renders twice that many samples with half phase increments.
   Actual archive vtable slots resolve these virtual calls; their identities are not
   inferred from Windows slot ordinals.
2. `Spectral_c::initModules` (0x7e590) creates seven stereo DS buffers at object offset
   0x678720, each with double block capacity. `SpectralTone_c::create` passes that base
   to the generator; `GenSpectral_c::cachePara` maps route 0..6 to those buffers.
3. `SpectralInst_c::renderAll` (0x9d2e0) prepares those buffers for 2N samples.
   `initModules` registers each DS buffer as the input of its corresponding Downsample
   module (module IDs 0x252..0x258), with output routed to a normal-capacity FX/OUT buffer.
4. `SpectralInst_c::render` (0x9d3d0) calls all seven downsamplers with N before the
   selected effect slots, master EQ and output stage. `Downsample_c::render` (0xdb3f0)
   reads two consecutive floats per channel, updates its low-pass states twice, and
   writes one result per channel. Its output virtual slot resolves to
   `Part_c::writeOutR` (0x7600), which writes and advances by one float.

**Conclusion: Downsample_c performs the synth's internal 2:1 conversion before the effect
chain. Crush6_c is the separately selectable bitcrusher.** This establishes the graph,
not equivalence between Partials' own FIR decimator and the original's IIR/DC-removal
stage. Partials currently enables 2× synthesis only for possible cross-modulation;
the original's generator path does it for ordinary oscillator/filter rendering too.

Gain identities are clearer as well. `getParaSetPtr` declares UI Osc Volume at parameter
offset 0x930; its linear gain at 0x940 flows through `cachePara`'s default stream 0x28,
`SpectralTone_c::premodulate`/`recordMod`, then generator stream index 10 and smoothed
field 0x924. It multiplies the waveform **before** the resonator. The oscillator amp
envelope feeds stream index 2 / field 0x920, also before it. Fil Volume is declared at
0x1770; its linear gain at 0x1780 reaches cached 0xe08 and smoothed 0xe18 in the
post-filter output product. The additional matrix factor at stream index 16 / 0x928
is also post-filter. Exact UI labels/scales for every separate Gain/Volume matrix
destination still need their own control-map check; these findings do not collapse them.

**Runtime cross-check.** `probe_archive_reference.py identity` produced seven raw WAVs
byte-for-byte identical to the preserved old references: Filth, fn HiHat, both Tortured
Lead cases, and isolated Filth oscillator 1 normal/bypass/no-resonance. Demo noise remains
present and is part of each equality. Earlier measurements for these cases are usable;
this does not validate every preset or every old measurement.

The first 15 controlled probes left Filth nearly sinusoidal after disabling PM. Their
32.7 Hz fundamental lies far below the 221 Hz cutoff; quiet RMS is increasingly dominated
by demo noise. The prominent fundamental follows the expected volume steps. These cases
are not adequate evidence of a resonant compression law.

The next 20 probes use our own 16-harmonic saw, flat spectral curve, MIDI 48 (130.813 Hz),
523.25 Hz cutoff, fixed envelopes, no modulation/effects, 48 kHz, Main -18 dB and Fil
Volume -12 dB unless stated. Analysis projects individual harmonics over 0.2–0.8 s;
reference WAVs are never edited. Fourth-harmonic boosts relative to the matching dry case:

| Osc Volume | Resonance | Original boost | Partials boost |
|---|---:|---:|---:|
| -12 dB | 44 | 11.09 dB | 12.19 dB |
| -12 dB | 75 | 17.86 dB | 21.61 dB |
| -12 dB | 100 | 19.55 dB | 26.36 dB |
| 0 dB | 44 | 2.65 dB | 12.19 dB |
| 0 dB | 75 | 2.50 dB | 21.61 dB |
| 0 dB | 100 | 2.52 dB | 26.36 dB |
| +12 dB | 44 | 2.06 dB | 12.19 dB |
| +12 dB | 75 | 2.82 dB | 21.61 dB |
| +12 dB | 100 | 3.01 dB | 26.34 dB |

Dry fourth harmonics match within 0.08 dB. The original's wet fourth harmonics in this
table stand 38–53 dB above the nearby spectral noise estimate. At 0 dB Osc Volume /
75 resonance, Partials' fourth harmonic is **19.17 dB louder** in absolute level.
This is a controlled probe result, not a claim that Filth Bass itself has a 19 dB error.

The decisive gain-placement pair is Osc -12 / Fil -12 versus Osc 0 / Fil -24. Both have
the same total dry gain and effectively identical dry fourth harmonics. At resonance 75,
the original's fourth harmonics differ by **15.35 dB**, while Partials' differ by less
than 0.000001 dB. Moving output volume cannot reproduce this behavior. Reducing Fil Volume
alone by 12 dB lowers the original's resonant fourth harmonic by 12.00 dB. Rich-probe
reference peaks remain below 0.387, so final output limiting does not explain the difference.

LP+ probes agree with the need to retain separate single/spread branches. At offset 0,
single-voice fourth-harmonic boost is 4.56 dB (Partials 12.20); spread gives 11.46 dB
(Partials 12.02). Spread projections contain detuned partials and are window-sensitive:
use them with the traced topology, not as a precise fit for the complete stereo response.

All 42 reference renders and their preset hashes/frame counts were checked by
`tools/summarise_archive_reference.py`. Manifests, untouched WAVs, scalar analyses and
`summary.json` live in `renders/probe_archive_reference/`. The unchanged Partials renderer
at analysis has SHA-256 `e1415e5d27cdb09eab0cae37c60d5435040561fdef066ec97d119e35a6e472fa`.
An initial summary check expected an explicit sample-rate field in the older manifest;
it was corrected to honor the runner's 48 kHz default and then passed. No renders were
repeated or changed for that bookkeeping correction.

**Implementation target, not yet delivered:** independently model nonlinear feedback
with oscillator gain/envelope before it, filter volume after it, the verified LP+ branch
order and suitable internal sampling. Do not port the decompiled update routine or fit a
post-output gain fudge. First validate a candidate on these drive pairs; then use different
cutoffs, waveforms, note attacks, single/spread and named presets as held-out checks.
Only then replace engine code and run normal/sanitizer, processor stress and CPU checks.
The current tests/app are unchanged; no new synth build is claimed by this research.

### Independent nonlinear candidate, validation and app integration (9–10 November)

The preceding implementation target is now built in Partials. Its own trapezoidal
band/low integrators use a cubic loss, solving
`band * (denominator + loss * band^2) = rhs` with four bounded Newton iterations.
This differs from the original's explicit update/saturator and was derived/fitted as
an independent model. Five fitted constants: damping scale 1.12969, damping offset
-0.11, cubic loss .1825, frequency scale .9825 and resonant mix scale 1.96562 (the engine
uses half of the latter against its doubled mix expression). Gain/amp precede the
resonator, Fil Volume follows it, and single/spread LP+ branch placement is retained.
The existing 2x synthesis path now also activates for resonance and modulated resonance.

`partials/tools/fit_resonator.cpp` fits nine rich-saw cases, twelve harmonic magnitudes
per case, to preserved scalar targets. Final training RMS is **0.188379 dB**. Frozen
parameters on fresh archive references at other cutoffs give **0.190441 dB** at 262 Hz
and **0.284084 dB** at 1047 Hz. Those held-out renders are under
`renders/probe_resonator_validation/` (18 cases: two saw cutoffs and a sine, three drives,
resonance zero/75). Reference hash/frame provenance is retained; demo noise is untouched.
The isolated sine's fundamental and odd harmonics match closely, but the original's weak
even harmonics are absent from our symmetric model. The full unweighted sine log-magnitude
error is about **60.69 dB**, so it cannot be represented as another successful full-spectrum
match. This is a measured limitation, not a reason to discard inconvenient harmonics.

`partials/tools/compare_resonator.py` renders only Partials for 45 preserved reference
cases. Results: single rich-probe prominent-harmonic errors generally .1–.45 dB, held-out
engine cases up to .82 dB; spread projections 1.17–1.81 dB, with detuned/windowing limits.
Early RMS differences for the user-reported patches: Filth Bass +.60 dB, fn HiHat +.88 dB,
Tortured Lead 1 -.41 dB, Lead 2 +.10 dB; isolated Filth oscillator 1 +.13 dB. RMS closeness
is not timbral or phase equivalence. The sine's missing even components also remain
visible in the integrated comparison. Raw outputs/JSON and prototype targets/logs live in
`partials/build/resonator_comparison/` and `partials/build/resonator_fit/`.

Integration passes 393 core checks including ASan/UBSan, four importer tests, and general
plus named-preset processor qualification. A native Spectral-layout editor and local Caps
Lock typing keyboard were added for Yan's own exploration. It exposes implemented patch
controls and preserves edits in `.ptch`/host state. Original assets/code are not embedded.
`partials/docs/design.md` records UI scope, remaining unsupported controls, final build
hashes/log locations and real-time deadline overruns. No AU installation, Live interaction,
noise bypass or commit was performed.
