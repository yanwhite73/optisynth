# Other digital-module sources — 17 November 2026

Yan requested respected modular makers with reusable open-source code, then
asked that the findings be logged and the project committed/pushed regularly.
This is a research shortlist; no code from these projects has been adopted or
executed. Full source/dependency/manual review remains required before use.

| Maker / project | Potential use in Optisynth | Published licence / evidence |
| --- | --- | --- |
| **4ms + Matthias Puech: Ensemble Oscillator** | Distinctive 16-oscillator engine combining additive, FM, phase distortion and wavefolding, with harmonic/scaled pitch organisation | [Product description](https://4mscompany.com/enosc.php), [official source](https://github.com/4ms/enosc), [BSD two-clause terms](https://github.com/4ms/enosc/blob/main/LICENSE). Hardware STM32 build needs a desktop DSP adapter; dependency review pending. |
| **4ms: Dual Looping Delay** | Clocked delay/looper and feedback processor | [Official source and README](https://github.com/4ms/DLD): software MIT; hardware CC BY-NC-SA. README has separate branding guidance. Port the DSP after review, not the hardware-flashing toolchain. |
| **Electrosmith / Daisy: DaisySP** | Oscillators, envelopes, filters, drums, granular playback, physical modelling and utilities | [Official source](https://github.com/electro-smith/DaisySP), [README](https://raw.githubusercontent.com/electro-smith/DaisySP/master/README.md): MIT DSP library explicitly supports plugins and VCV modules. Some algorithms derive from Mutable; check provenance to avoid mistaking duplicate implementations for new engines. Current GitHub redirects to daisyaudio/DaisySP. |
| **Rebel Technology / Befaco: OWL / Lich** | Programmable synth/effect patches and modular DSP examples | [OwlProgram](https://github.com/RebelTechnology/OwlProgram) SDK GPL-3.0; [Befaco Lich](https://www.befaco.org/lich/) confirms OWL platform. Individual patch and dependency licences need separate checking. C++, Faust, Pure Data and Max Gen routes exist. |
| **Befaco via VCV** | Software models of Eurorack oscillators, waveshapers, envelopes and utilities | [Official VCV repository](https://github.com/VCVRack/Befaco), [licence](https://github.com/VCVRack/Befaco/blob/v2/LICENSE.md): source GPL-3.0-or-later, panel graphics separately permissioned, component graphics CC BY-NC 4.0. These are software models, not necessarily the original hardware's digital firmware. |
| **Music Thing Modular: Radio Music** | Sample-player behaviour and performance controls | [Author's source](https://github.com/TomWhitwell/RadioMusic), [README](https://raw.githubusercontent.com/TomWhitwell/RadioMusic/master/README.md): hardware/software design CC BY-SA 3.0; included Teensy work and suggested sample content explicitly excluded from that licence. Firmware/Teensy dependencies require individual review. |

Priority recommendation (engineering judgement): continue Mutable integration,
then review 4ms Ensemble Oscillator for a distinctive new engine and DaisySP for
additional interchangeable building blocks. Delay/looper work follows the graph's
voice/global boundary. No commitment to adopting all candidates.

Public availability and permission to reuse must be checked per file and dependency;
product names and panel artwork are separate from algorithm code. Instruo's VCV
collection is listed as [freeware](https://library.vcvrack.com/Instruo), so it is not
currently on the confirmed reusable-source shortlist. Bastl's public Kastle tree
was found but its applicable licence was not established in this pass; leave it
as a research lead rather than an adoption candidate.

## Pre-use review still outstanding

For any selected candidate, read its available manual, README, release notes and
complete relevant DSP/dependency closure. Pin a revision, record notices, map
sample-rate/block assumptions and controls, and document side effects. Embedded
firmware can read/write SD cards, EEPROM and hardware ports; only reviewed buffer-
based DSP should enter our callback. No third-party installer, flashing command,
network listener or MIDI/device-control program was run for this survey.
