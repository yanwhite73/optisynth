# Classic pad synth architecture — circuit-level notes

Request 51, 19 November 2026. These notes come from schematics and service
manuals, not from parameter lists. The Prophet-5, SEM, OB-X, OB-Xa,
OB-8 and Xpander voice architectures are covered; the gaps are listed at the end.

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

## Oberheim SEM (1974)

Source: `SEM_1A_Schematics.pdf`, pages 1–5, read directly. Parts are 5% unless
marked otherwise.

**VCOs (two).**
- Discrete sawtooth cores: CA3086 transistor-array exponential converters with
  a 1 kΩ temperature-compensating resistor, and 1000 pF polystyrene timing
  capacitors.
- Pulse comes from a 301A comparator.
- **Only saw and pulse**; there is no triangle output.
- The MOD pot selects LFO, ENV1 or external, and applies it either as FM or
  as PWM.

**VCF.** The classic 2-integrator state-variable filter.
- An input summer (301A) feeds two **CA3080 OTA integrators** with 300 pF caps.
- A transistor-pair exponential converter drives both OTAs.
- Outputs are high-pass (summer), band-pass and low-pass (buffered integrators).
- **Resonance runs through a pair of anti-parallel 1N4148 diodes** in the
  damping loop (D11/D12 with 100 kΩ and 47 kΩ). The diodes soft-limit the
  feedback, so resonance compresses rather than screaming.
- The MOD pot is bipolar, selecting ENV2, LFO or external.

**Mode.** A 50 kΩ "NOTCH" pot crossfades from the HP output to the LP output,
with the notch at its centre. A switch substitutes BP for the crossfade.
This continuous LP–notch–HP sweep is the SEM's signature.

**Envelopes (two).** Discrete ADS: attack, decay, sustain.
- **Release equals decay** (on gate off, the envelope falls at the decay rate).
- Timing uses a 4.7 µF capacitor with 1 MΩ pots: RC charging, with time
  constants up to about 4.7 s.
- Gate and trigger modes.

**VCA.** A CA3080 OTA. Its input divider is 100 kΩ/220 Ω, which keeps the OTA
close to linear but leaves some tanh character. It is driven by ENV1, or held
fully on.

**LFO.** One discrete LFO with a 1 MΩ rate pot.

## OB-X (1979)

Source: `OBX_Service_Manual.pdf`, page 20, "OB-X voice card" dated 30 June
1979, read directly. The text pages also describe per-voice pan pots on the
mother board and voice-card trims.

- **Oscillators:** SEM-style discrete VCOs (CA3086 exponential converters,
  1000 pF polystyrene caps). Waveforms are selected by 4053 switches, and
  oscillator 1 has sync.
- **Filter:** a 2-pole state-variable, as in the SEM. CA3080 integrators with
  330 pF mica caps, a CA3080 resonance VCA, and the **same anti-parallel diode
  limiter** in the resonance loop. Noise enters at the input summer.
- **Envelopes:** two CEM3310 (filter and amp).
- **VCA:** CA3080.
- **Character:** the SEM voice made programmable, still 12 dB with
  diode-limited resonance, now with CEM envelopes.

## OB-Xa (1981)

Source: `OBXa_Schematics_1st_Ed.pdf`, page 19, "OB-Xa voice card" dated 1 March
1981, read directly.

- **Oscillators:** two CEM3340, with 0.001 µF polystyrene caps. Waveforms are
  selected by 4053 switches; sync is through a 4016.
- **Two separate CEM3320 filters per voice:**
  - One labelled "2 POLE VCF", wired as a state-variable with 300 pF mica caps.
    Its resonance has an anti-parallel diode pair and a "RES ADJ" trim.
  - One labelled "4 POLE VCF", the standard 4-stage cascade with four 300 pF
    mica caps.
  - A 4053 switch selects which one feeds the VCA. Each filter has its own
    V/oct and frequency trims.
- **Envelopes:** two CEM3310.
- **VCA:** CA3080, with a per-voice volume input.
- **Other inputs:** noise enters the filter through 620 kΩ. There is a switched
  filter-modulation path ("FMOD"); its source has not been traced yet.

## OB-8 (1983)

Source: `OB8_Service_Manual.pdf`, page 21, voice board sheet 3 of 3, read
directly.

- **Oscillators:** two CEM3340 with polystyrene timing caps. Saw, square and
  triangle are switched by 4016s; sync is available.
- **One CEM3320 per voice,** reconfigured between 4-pole cascade and 2-pole
  state-variable by 4053/4016 switches labelled "4P". Anti-parallel diodes sit
  in the resonance network. This replaces the OB-Xa's two filter chips, and
  agrees with Electric Druid's description.
- **Envelopes:** two CEM3310.
- **VCA:** CA3080 with a TL081 output stage.
- Resistors marked "x" are numbered per voice; resistors marked "\*" are
  matched 1% parts.

## Xpander / Matrix-12 (1984)

Source: MAME `xpander_vb.cpp` (BSD-3), transcribing voice-board sheets 3–4. The
local service manual has not yet been cross-checked.

- **Voice chips:** a CEM3374 dual VCO and a CEM3372 (mixer, 4-pole low-pass and
  VCA). Oscillator 2 can modulate oscillator 1 or the filter, with FM depth set
  by a per-voice MDAC (AD7523). Noise is switched into the oscillator 2 channel.
- **15 filter modes from one 4-pole low-pass:**
  - Each stage's capacitor voltage is buffered and scaled:
    - stage 1: ×101;
    - stage 2: ×−101;
    - stage 3: ×101;
    - stage 4: ×−1, taken from the chip output.
  - A CD4051 selects one of **8 resistor mixes** of the four stages.
  - Separately, a CD4053 swaps the first stage's capacitor from 0.033 µF to
    100 pF, which makes stage 1 a pass-through. 8 mixes × 2 = 16 combinations,
    of which 15 are unique.
- **The 15 modes:** 4-pole LP, 3-pole LP, 2-pole LP, 1-pole LP; 3-pole HP,
  2-pole HP, 1-pole HP; 4-pole BP, 2-pole BP; 2-pole notch; 3-pole all-pass;
  and combinations with an extra 1-pole LP (3 HP + 1 LP, 3 phase + 1 LP,
  2 notch + 1 LP, 2 HP + 1 LP).
- **Panning:** each voice has its own **8-position pan**, a switched resistor
  ladder (10–56 kΩ) into separate left and right summing amps. Voices sit at
  different stereo positions.
- **Modulation:** all LFOs and envelopes are digital, delivered as multiplexed
  sample-and-hold control voltages from a 14-bit DAC.

## Comparison: what makes these pads lush

| Feature | SEM | OB-X | OB-Xa | OB-8 | Xpander | Prophet-5 Rev 3 |
| --- | --- | --- | --- | --- | --- | --- |
| VCO | Discrete saw core | Discrete saw core | CEM3340 | CEM3340 | CEM3374 | CEM3340 |
| Filter | OTA 2-pole SVF, LP–notch–HP mix | OTA 2-pole SVF | CEM3320 2-pole SVF **or** 4-pole (two chips) | CEM3320 switched 2-/4-pole | CEM3372 4-pole + 15-mode pole mix | CEM3320 4-pole |
| Resonance limiting | Diode pair | Diode pair | Diode pair (2-pole) | Diodes | Chip internal | Chip internal |
| Envelope | Discrete ADS (release = decay) | CEM3310 ×2 | CEM3310 ×2 | CEM3310 ×2 | Digital | CEM3310 ×2 |
| VCA | CA3080 | CA3080 | CA3080 | CA3080 | CEM3372 VCA | CA3280 |
| Stereo | — | Per-voice pan pots | Not traced | Not traced | 8-step pan per voice | Mono |
| Per-voice FM | VCO mod switch | — | Filter mod switch | Filter mod switch | Osc 2 → osc 1 or VCF (MDAC) | Poly-mod |

The Oberheim 12 dB sound is a 2-pole state-variable whose resonance is softened
by diodes. It keeps more low end and harmonics than a 4-pole low-pass, so pads
stay full and open as the filter sweeps. The OB-Xa and OB-8 keep that sound and
add a 4-pole option. The OB-X (pan pots) and the Xpander (8-step pan) place each voice separately,
so chords spread across the stereo field without any chorus. Panning on the
OB-Xa and OB-8 is not yet traced. The Prophet-5 is
mono and 4-pole; its movement comes from poly-mod and the shared wheel-mod LFO.

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

7. Build an OTA state-variable filter with a diode-limited resonance loop and a
   continuous LP–notch–HP mode control (SEM, OB-X, OB-Xa 2-pole), alongside the
   4-pole OTA cascade.
8. Add a pole-mixing output stage to the 4-pole filter: tap each stage, mix
   with the Xpander's 8 resistor sets, and switch stage 1 to pass-through. One
   filter model then gives 15 modes.
9. Pan each voice to its own fixed stereo position (OB-X, Xpander) before any
   chorus.

## Still to do

- **Exact values not yet transcribed:** the SEM resonance diode network, the
  OB-8 2-pole/4-pole switch topology and the Xpander mix resistor table. The
  table is in MAME `xpander_vb.cpp` `R_MIX`; check it against the local
  service manual.
- **Signal sources not traced:** the OB-Xa and OB-8 "FMOD" filter-modulation
  sources, and the OB-8 per-voice pan circuit.
- **Prophet-5 Rev 2:** the SSM chips need separate study.
- **Measurements:** none of this has been checked against hardware. Filter
  drive, resonance limiting and envelope curves need A/B comparison with
  renders before claiming a match.
- **On decompiling Arturia:** not pursued. The schematics, MAME and the
  datasheets are better and freely readable sources, and Arturia's licence
  terms on reverse engineering have not been checked.

## Mix-and-match modularity, performance and latency (request 52)

**Nothing fundamental prevents it.** Each of these synths is already modular
inside: separate chips (VCO, filter, envelope, VCA) joined by voltages. Software
modules can be joined the same way.

**What must stay inside one module.** Tight loops and internal taps:
- the filter resonance loop, including the diode limiting;
- the Xpander pole taps (pole mixing is a filter feature, or the filter must
  expose its four stage outputs);
- OB-8 2-pole/4-pole reconfiguration;
- the SEM mode crossfade;
- each oscillator's own waveform mixing.

**What has to cross modules, and the runtime does not support yet:**
- **Audio-rate modulation inputs:** Prophet poly-mod, oscillator FM, filter FM.
  The matrix currently samples once per 24-frame block.
- **Hard sync:** a dedicated port carrying a sub-sample reset time.
  Band-limited oscillators need the fractional reset position.
- **Per-voice graphs:** polyphony.
- **An explicit shared/per-voice split:** for example, the Prophet's single
  wheel-mod LFO is global, while its poly-mod is per voice.
- **Explicit causal feedback:** the planner rejects cycles.

**Level convention.** Saturation depends on drive level, so modules from
different instruments only drive each other correctly with a shared voltage
convention: for example ±5 V audio and 1 V/oct, as in VCV Rack. Each module
then scales its own inputs as its schematic does. Without this, an SEM filter
fed by a Prophet oscillator would be under- or over-driven. Component
tolerance and drift should be a per-voice seed that every module reads, so
voices stay individual whatever the combination.

**Performance (measured 19 November 2026).**

Test setup:
- **Code:** scratch benchmark at
  `scratchpad/bench/voice_bench.cpp`; not project code.
- **Voice model:** two PolyBLEP saws, a nonlinear 4-pole OTA cascade or a
  diode-damped SVF at 2× oversampling with 31-tap halfband resampling,
  per-sample envelope cutoff modulation, and two envelopes.
- **Machine and settings:** Apple Silicon, single thread, 48 kHz, `-O2`. The
  host was under memory pressure, so expect some noise.

| Voices | Ladder, `std::tanh` | Ladder, fast tanh | SVF, `std::tanh` | SVF, fast tanh |
| --- | --- | --- | --- | --- |
| 1 | 0.32% of one core | 0.26% | 0.26% | 0.25% |
| 8 | 2.6% | 2.1% | 2.2% | 1.9% |
| 16 | 5.3% | 4.1% | 4.3% | 3.9% |

These are cheap explicit nonlinear models, not zero-delay-feedback Newton
solves or MAME-style component models. Even 10× this cost (about 25% of one
core for 8 voices) would be workable.

Further gains:
- Run each module across all voices at once with SIMD, as VCV Rack does.
- Oversample the whole voice once rather than inside every module.

Calling modules once per 24-sample block costs very little next to the DSP.

**Latency.**
- **Feed-forward chains** (oscillator → filter → VCA) add none: modules run in
  dependency order within the same block.
- **Feedback routed through the graph** would cost a block: 24 samples, 0.5 ms.
  That is fine for slow modulation, but it changes the sound of audio-rate
  feedback. Keep such loops inside modules, or process the cycle per sample.
- **2× oversampling:** a linear-phase 31-tap halfband adds about 15 samples at
  the high rate (~0.16 ms); IIR halfbands add only a few samples.
- **The existing output path** adds about 0.8 ms (the 24-sample quantum plus
  ~16 samples of rate conversion), on top of the audio device buffer.
- None of this is perceptible for pads; the device buffer dominates playing
  latency.
