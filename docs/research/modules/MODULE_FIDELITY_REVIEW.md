# Module fidelity review — request 115

24 November 2026, Claude session, on 0.0.29.

Yan: "we still dont have any modules that look or feel like deep ones such as
mimeophon, vhikk x or other delay/reverbs that have been logged. Also the mutable
ones should have similar layout. It really helps to differentiate modules and see
what in/out options are available."

## Summary

- **No complete module has been recreated.** Six modules wrap original
  open-source DSP components inside our own controls. Two more are behavioural
  studies written from manuals. Neither group reproduces the original's controls,
  inputs and outputs, or panel.
- **No panel follows its original layout.** Every module uses the same generic
  card: a row of sockets at the top, then a grid of knobs. That is why modules
  are hard to tell apart and inputs are hard to tell from outputs.
- **One deep space module exists.** Prism tank adds one mode of one module (the
  Clouds Parasites reverb tank) plus our own echo.
- **The largest step is open source.** Beads and Clouds (MIT) supply deep
  delay, granular and reverb processing. Their panels can be followed in
  arrangement, drawn by us.
- **Mimeophon can be approached** from its detailed manual. Vhikk X is the least
  documented of those named.

## 1. What has been recreated

| Optisynth module | Upstream | DSP | Controls and sockets compared with the original | Panel |
| --- | --- | --- | --- | --- |
| VA oscillator | Plaits `VariableShapeOscillator` (MIT) | Original component, unchanged | One internal part of Plaits' virtual-analog model. Pitch, shape, pulse width, sync ratio, level. Plaits' Harmonics / Timbre / Morph, attenuverters, Trigger, Level and Aux are absent | Generic card |
| Sine oscillator | Plaits `SineOscillator` | Original component | Utility part, not a Plaits model | Generic |
| Modal percussion | Plaits `ModalVoice` | Original component, deterministic trigger path | The core of Plaits' modal model. Pitch, material, brightness, decay. The Aux (exciter) output and Plaits' parameter mapping are not reproduced | Generic |
| Modal resonator | Plaits `Resonator` | Original component | 24 modes excited by an external input. **Not** Rings: no structure / position, no polyphony, none of Rings' six resonator models, no odd / even outputs | Generic |
| Bass drum | Plaits `AnalogBassDrum` | Original component | Pitch, tone, decay, attack FM; close to the component's own arguments | Generic |
| Digital SVF low / band / high-pass | stmlib `Svf` (MIT) | Original library filter | A library utility, not a Mutable module | Generic |
| Prism tank | Clouds Parasites Oliverb / Miverb (MIT) | Original tank, rescaled from 32 kHz to 48 kHz, with our own clocked echo | One of Parasites' six Clouds modes. Its controls are our own mapping of Clouds' knobs | Generic |
| Dual looping delay | 4ms DLD **manual** (source not used) | Independent model | Selected documented behaviours; not the DLD firmware | Generic |
| Dual low-pass gate | Make Noise Optomix **manual** | Independent model | Two gates, strike, control and damp; no side-chain or vactrol measurements | Generic |

So Optisynth uses the cores of about three of Plaits' 24 models and one of the
six Clouds Parasites modes. That is it for the 28 Mutable modules whose files are
published. The analog OB / Prophet voice is a separate track, modelled from
service manuals and schematics, and is outside this review.

## 2. Panels that show what a module is

The Mutable panels have a recognisable hierarchy:
- Large knobs for the main sound parameters, medium knobs for secondary ones,
  small attenuverters beside their CV inputs.
- Buttons and LEDs for modes.
- All sockets in rows at the bottom, with outputs marked apart from inputs.

Examples:
- **Plaits:** Frequency and Harmonics large; Timbre and Morph medium; three
  attenuverters; model buttons; ten sockets ending in Out and Aux.
- **Rings:** Frequency and Structure large; Brightness, Damping and Position
  medium; In, Strum and V/Oct in, Odd and Even out.
- **Clouds:** Position, Size and Pitch large; In gain, Density, Texture and
  Blend; Freeze; stereo in and out.

Sources: the [Plaits](https://pichenettes.github.io/mutable-instruments-documentation/modules/plaits/manual/),
[Rings](https://pichenettes.github.io/mutable-instruments-documentation/modules/rings/manual/),
[Clouds](https://pichenettes.github.io/mutable-instruments-documentation/modules/clouds/manual/) and
[Beads](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/) manuals.

**What the licences allow.** The [eurorack repository](https://github.com/pichenettes/eurorack)
puts STM32 code under MIT, AVR code under GPL-3.0 and hardware files under
CC BY-SA 3.0. It says the Mutable Instruments name must not be used on derivative
works, and recommends not keeping the original module names.
- **Allowed:** following the hierarchy and placement (which controls are large,
  which sockets sit where, inputs versus outputs).
- **Not allowed:** tracing or reusing the panel artwork (share-alike) or the names.
- **Therefore:** our own drawings and names, with "after the original design"
  provenance in the docs.

**Proposed mechanism.** A panel description per module, in our own code:
- width in rack units;
- each control's role and size (large, medium, small attenuverter, button,
  LED, switch);
- sockets in a bottom row with inputs and outputs styled differently;
- grouping lines.

It would be laid out with JUCE Grid and FlexBox, like the current cards, so text
keeps its readable size and density changes only control size and spacing.
Modules without a description keep the generic card. The same description can
drive the rack card, the focus view and the synth panel. That makes in / out
options visible on every module, not only Mutable ones.

## 3. What else can be approached, by what is known

**A. Source published under an adoptable licence: a faithful port is possible.**

| Module | What it would add | Licence (checked 24 Nov) |
| --- | --- | --- |
| Beads | Granular, delay and reverb processing with feedback; a synthesis mode with no input | MIT (STM32). The repository notes the code is provided as-is and builds on desktop |
| Clouds (all modes) | Granular, pitch-shifter / time-stretch, looping delay, spectral; Parasites adds the reverb and a resonator mode | MIT |
| Rings | Six resonator models, polyphony, odd / even stereo | MIT |
| Elements | Bow, blow and strike exciters into a modal resonator with reverb | MIT |
| Plaits (all models) | 24 synthesis models behind one panel | MIT |
| Warps, Tides 2, Stages, Marbles, Braids, Frames | Cross-modulation, function generation, sequencing, random voltages | MIT (STM32 projects) |
| 4ms Dual Looping Delay | The real firmware behaviour, replacing our manual-based study | Code MIT, hardware CC BY-NC-SA ([repository](https://github.com/4ms/DLD)) |
| 4ms Spectral Multiband Resonator | Tuned filter-bank resonator | Code MIT; do not use the 4ms name ([repository](https://github.com/4ms/SMR)) |
| 4ms / Puech Ensemble Oscillator | 16-oscillator swarm (the Rich swarm study) | BSD-2-clause ([repository](https://github.com/4ms/enosc)) |
| Airwindows reverbs | Galactic and others as single effects | MIT |

**B. Source published but GPL: study only under [LICENSING.md](../../project/LICENSING.md).**
Valley Plateau and VCV's Mutable wrappers (Audible Instruments). They are useful
for control behaviour and rate / block handling, not for code.

**C. Closed DSP with a detailed manual: behavioural models.** These can be
convincing in function and controls. Matching the sound needs recordings or the
hardware for A/B comparison.
- **Make Noise Mimeophon.**
  - Eight nested time zones from about 1.3 ms (Karplus-Strong) to 41 s; every
    zone is always recording.
  - Rate and μ (Doppler, 1 V/oct in the short zones).
  - Repeats with Color in the feedback path, Halo diffusion, Skew (opposed
    left / right rates), ping-pong, Flip (reverse) and Hold.
  - Tempo input and rate outputs.
  - Source: [manual](https://cdn.shopify.com/s/files/1/0234/8001/files/mimeophon-manual.pdf?98).
- **Make Noise Erbe-Verb, Echophon and Morphagene.** Manuals published; DSP closed.
- **Make Noise Optomix.** An analog vactrol low-pass gate. The behaviour is well
  understood, and our study already covers its main controls.
- **Intellijel Sealegs, Strymon StarLab, WMD Cosmic Debris.** Manuals published;
  DSP closed.

**D. Closed with little published technical detail: Forge TME Vhikk X.**
- Public descriptions give a stereo voice and processor with nine algorithms.
  The effect engines are named Dungeon (a metallic delay / reverb) and Reflekta
  (a diffuse delay structure). Two encoders, CV throughout, 24-bit stereo
  in / out.
- No manual or design description was reached in this review (the
  manufacturer's site did not respond).
- Only an "inspired by" design from the manual, if available, and recordings is
  possible. Fidelity here would depend on Yan's own recordings or hardware.

## 4. Recommended order (proposal; Yan to confirm)

1. **Panel descriptions.**
   - Build the panel mechanism, and give Prism tank, the modal modules and the
     drum hierarchy-faithful panels.
   - Mark outputs distinctly on every module, including generic cards.
2. **A Beads-derived processor** as the first deep delay / reverb / granular
   module:
   - full controls, stereo in / out and clocking;
   - its panel following the original's arrangement.
3. **A complete Plaits-derived macro oscillator** with all models and the
   original's control set.
   - The existing VA, modal and drum modules stay for older patches.
4. **Rings- and Elements-derived resonators**, replacing the partial modal
   resonator as the main resonator.
5. **Port the DLD firmware** behind the dual looping delay; keep the panel.
6. **A Mimeophon-style repeater from its manual.**
   - Zones, Color, Halo, Skew, Flip and Hold.
   - A/B with recordings if Yan can supply them.
7. **Vhikk X:** wait for the manual or recordings before designing.

Each port follows the existing rules:
- pin the revision and read the relevant source and manual first;
- keep MIT notices;
- use our own names;
- convert 32 kHz designs to 48 kHz deliberately;
- run offline render checks before Yan listens.

The review above records the state at 0.0.29.

**26 November, 0.0.30.** Step 1 shipped in the module rack and focus view.
Eight panel descriptions set each row’s controls, knob sizes and grouping.
Prism tank groups space, echo, pitch/motion and returns. All rack outputs
use an inverse jack plate and a directional mark. The curated preset front
panel still groups playing gestures across modules. The modules retain
their previous DSP and ports. Step 2, the Beads-based processor, is next.
