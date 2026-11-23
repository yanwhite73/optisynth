# Research index and evidence

Research and implementation are parallel parts of Optisynth. Record what was
actually inspected, the source edition/page or code revision, the inferred DSP
mechanism, uncertainty and the test that could disprove it. A link found is not
a document analysed. The first reviewed Mutable DSP subset is now adopted; see
`modules/MUTABLE_FIRST_SLICE.md` and `../../third_party/mutable/PROVENANCE.json`.

`research_sources.json` is the source catalogue. Local manual/schematic copies go
under `../references`; `reference_downloads.json` records their URLs and SHA-256.
Reference PDFs/ZIPs are git-ignored; the catalogue, receipts and research notes are
kept with project source. ZIP contents are not executed or automatically extracted.

Initial collection complete: all five indexed documents/archives are stored
locally (37,952,171 bytes total), header-checked and hashed. Download success does
not change the per-source inspection status: detailed schematic reading remains
pending. A pinned subset of the original Mutable DSP is vendored; VCV remains
reference only, and no upstream firmware build scripts were executed.

## Oberheim Xpander

Request 47 survey: [Oberheim and Prophet sources](instruments/OBERHEIM_PROPHET_SOURCES.md)
covers service manuals and schematics, commercial plugins and open-source models
for the SEM, OB-X/OB-Xa/OB-8, Xpander/Matrix-12 and Prophet-5. Key finding: MAME
has BSD-3-Clause component-level CEM3310/3320/3340/3372 models, a Prophet-5 Rev 3.0
driver and a partial Xpander voice board (filter and 15-mode pole mixer; VCOs not
yet implemented). Survey only: nothing downloaded or adopted.


- [Owner manual](https://synthfool.com/docs/Oberheim/Oberheim_Xpander/Oberheim_Xpander_Owners_Manual.pdf).
- [Service manual](https://synthfool.com/docs/Oberheim/Oberheim_Xpander/Oberheim_Xpander_Service_Manual.pdf), 39-page scanned edition.
- [Separate schematic archive](https://synthfool.com/docs/Oberheim/Oberheim_Xpander/xpander_schematics.zip).
- [Alternate combined service manual](https://www.synfo.nl/servicemanuals/Oberheim/OBERHEIM_XPANDER_SERVICE_MANUAL.pdf).
- [Manufacturer manual filter page, readable mirror](https://www.manualslib.com/manual/3534905/Oberheim-Xpander.html?page=26).

Read evidence: the owner manual describes 15 filter modes, including different
slopes, band-pass/notch, phase and combined responses. This supports a reusable
multimode filter as a research target. It does not yet establish coefficients,
nonlinear drive, resonance gain or sample-rate behavior for an implementation.

Next: verify schematic sheet revision/completeness; trace the voice/filter circuit,
control scaling, mode switching and gain staging. Separate documented topology
from measured component/voice variation. Derive transfer responses, then test
frequency response, resonance and driven behavior before full-preset comparisons.

## Roland JP-8080

- [Official owner manual](https://static.roland.com/assets/media/pdf/JP-8080_OM.pdf).
- [Roland service notes, July 1998, archive mirror](https://www.synfo.nl/servicemanuals/Roland/JP-8080_SERVICE_NOTES.pdf).

Service-note index verified: block diagram page 13, circuit diagrams pages 17–27.
The specification lists Super Saw, Triangle Mod and Feedback OSC among the source
types. This is a digital/virtual-analogue research track: circuit diagrams can
explain converters, routing and surrounding electronics, but do not reveal the
internal oscillator/filter algorithms running inside the DSP chips.

Next: map owner-manual controls and modes; inspect the signal path and sample-rate
domains; identify independent measurements for detune/mix laws, phase behavior,
aliasing, filter responses and feedback. Start with useful character-inspired
modules; do not claim an exact JP emulation from feature names or schematics alone.

## Mutable Instruments and VCV Rack references

[VCV architecture and performance lessons](platforms/VCV_ARCHITECTURE_LESSONS.md) compares
primary engine/oscillator sources with our current modules, notes SIMD, scheduling,
UI/asset costs and proposes voice groups with explicit modulation/event interfaces.
It includes pinned revisions, current limitations and a future benchmark matrix.
No VCV code adopted or comparative performance claim made.

- [Original Mutable source](https://github.com/pichenettes/eurorack).
- [Rings manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/rings/manual/).
- [Plaits modal engine interface](https://github.com/pichenettes/eurorack/blob/master/plaits/dsp/engine/modal_engine.h).
- [VCV Audible Instruments](https://github.com/VCVRack/AudibleInstruments/tree/v2).
- [VCV Rings adapter](https://github.com/VCVRack/AudibleInstruments/blob/v2/src/Rings.cpp).

The original README marks STM32F code MIT and AVR code GPL3; the inspected Plaits
modal header contains an MIT notice. Original naming guidance is separate. Inspect
each selected module, resources and dependencies and pin its revision before
adoption. The VCV port's own licence is a separate check; do not infer it from
the embedded DSP's licence.

Rings documents modal, sympathetic-string and string resonators with either
external audio excitation or an internal pulse/noise burst. Damping, spectral
structure and excitation position are useful independently addressable controls
for percussion. This supports samples both as exciters and as parallel attacks.

The inspected VCV Rings adapter uses input/output resamplers around a 48 kHz,
24-frame internal DSP block. This is evidence that desktop adaptation involves
rate/block conversion, buffer sizing and event timing, not merely copying a render
function. It is a design pointer, not an adopted implementation or measured CPU result.

First selection implemented: Plaits ModalVoice/Resonator and AnalogBassDrum,
reviewed with their dependency closure, manual, licence and documentation index.
The original panel guides control hierarchy; our own contextual panels name each
algorithm's controls explicitly. The exact DSP needs no further circuit model to
be useful: AnalogBassDrum already includes a circuit-derived behavioural model.
Analogue-only Mutable modules are a separate research track. Desktop adaptation
still requires rate, event timing, gain, control-range and resource-bound tests.

The wrapper clamps the stiffness table index below its last entry because the
upstream interpolator always reads index+1. Only the sine and stiffness resource
arrays are retained, with a reproducible extraction and original notices.
Digital source runs at 48 kHz. A bounded windowed-sinc converter adapts output to
the device rate. The rack adds the original variable-shape/sine oscillators, an
externally excited Plaits resonator and stmlib digital SVF variants. This is the
library's digital filter, not an analogue Ripples circuit recreation. Review and
acceptance: [RACK_ROUTING.md](../architecture/RACK_ROUTING.md). Free-running dust remains disabled.
Next: more original engines, then granular processing.
Continue the same module-specific review before adoption.

Sample excitation is implemented in standalone 0.0.5 with a first-party WAV player.
[SAMPLE_PLAYER.md](../architecture/SAMPLE_PLAYER.md) records the inspected RIFF/WAVE specifications,
file/asset/worker contracts, tests and remaining interpolation/stereo limits.

Request 31 adds Pigments/Vital-style parameter assignment and matrix overview as
a UI option alongside typed module patching. The user-provided screenshot is stored
locally as `../references/pigments-modulation-user-20261117.png`; design implications
are in `../ui/CONTEXTUAL_UI.md`. Original product manuals and modulation runtime semantics
still need research before implementation; no product code is adopted.

## Other open-source digital-module makers

[OPEN_SOURCE_MODULE_MAKERS.md](modules/OPEN_SOURCE_MODULE_MAKERS.md) records the checked
4ms, DaisySP, Rebel/OWL, Befaco VCV and Music Thing sources, published licences,
potential uses and remaining pre-use review. None has been adopted in this build.

## Sequencer models and independent arpeggiator

[SEQUENCER_MODELS_RESEARCH.md](sequencers/SEQUENCER_MODELS_RESEARCH.md) is the expanded request
38 source study: 15 families, method taxonomy, primary source cards, licence and
inspection limits. [SEQUENCER_UI_ARCHITECTURE.md](../ui/SEQUENCER_UI_ARCHITECTURE.md)
synthesises shared Pattern modes/widgets and separate specialist modules, with
state, capture, agent and performance requirements. Fourteen repository inventories
and 86 downloaded reference files are indexed with hashes and inspection scope in
[sequencer_research_sources_20261117.json](sequencers/sequencer_research_sources_20261117.json).
Original source/manual/UI copies remain under ignored `../references`.

Request 41 corrects the earlier insufficient visual sample. The
[sequencer visual atlas](../ui/sequencers/README.md) adds 36 images to the previous
three, with all 39 visually inspected and annotated across eight source groups.
[Workflow analysis](../ui/sequencers/WORKFLOWS.md) distinguishes image observations,
manual-confirmed behaviour and our adaptations. The image catalogue has direct
URLs/hashes; the original bytes are separately backed up. This is published
visual research, not hands-on verification or permission to adopt artwork.

Request 42 adds [OXI ONE MKII](sequencers/OXI_ONE_REFERENCE.md) as a commercial
model/UX reference, with 15 inspected diagrams and 13 saved chapter snapshots.
Chord semantics, trigger-driven phrase traversal, stage duration, harmonic following
and persistent performance state sharpen the proposal. Reusable ONE engine source
has not been established; this does not change the open-source adoption boundary.
The combined visual atlas now has 54 images across nine source groups.

Corrections verified against source: PER|FORMER software is MIT; Marbles headers
are MIT but Grids is GPLv3-or-later; a current freeware Stellare listing does not
establish reusable source. Tidal's collected GitHub snapshot is historical after
its move to Codeberg. No sequencer candidate has been adopted or run.

[SEQUENCERS_AND_ARPEGGIATORS.md](sequencers/SEQUENCERS_AND_ARPEGGIATORS.md) retains the earlier
VCV/local Optiseq findings and the tested timing foundation. Request 38
superseded its old implementation order. The research itself changed no engine code.
Request 43 now implements first-party Pattern, Euclidean and Stage modules with
event routing and shared transport; see [the implementation contract](../architecture/FIRST_SEQUENCERS.md).
The independent held-note arpeggiator and later specialist models remain pending.

## Delay and reverb modules

[OPEN_SOURCE_DELAY_REVERB.md](modules/OPEN_SOURCE_DELAY_REVERB.md) records Clouds/Parasites,
4ms DLD, Valley Plateau, CHOW Matrix, Airwindows Galactic and DaisySP sources and
licences. It distinguishes original DSP from firmware/plugin wrappers and calls
out rate assumptions, stereo, tail ownership and causal feedback requirements.
No source from these candidates was adopted; full module-specific review remains
required before use. Standalone 0.0.6 now includes the complete first-party Spectral
engine, stereo routing and the first shared matrix; see
[SPECTRAL_ENGINE_INTEGRATION.md](../architecture/SPECTRAL_ENGINE_INTEGRATION.md). The next interface
work is sequencer/arp clock and timestamped events. New effect modules remain queued.

## Parallel project: repurposed Korg Wavestate

Yan noted a parallel Wavestate repurposing project as a possible physical controller
for Optisynth, or potentially a hardware host for its engine. Track both possibilities
alongside the desktop project. Controller mapping and running DSP on the instrument
are separate research questions; neither integration has been established here.

When that project's findings are available, record its location, exact hardware
revision, accessible controls/interfaces and hosting constraints. Use that evidence
to assess integration with Optisynth's stable module/parameter identities. Hosting
feasibility remains open; this note does not change the current implementation order.

## Required evidence per module

For each candidate, keep a small record: intended musical role; primary source
pages/code revision; control units and useful ranges; input/output and voice scope;
state/assets required; CPU/memory measurements; implementation status; remaining
uncertainties. For external code also record dependencies, side effects, ports,
licences and disable/removal path before use. Offline DSP should open no network,
MIDI or audio device by itself.

## First-party source boundary

The copied Spectral engine is our independent implementation, not LinPlug source.
Its observed architecture and known approximations are retained in
`../../reference/docs/spectral_architecture.md` and the inherited `../legacy/optispectral/design.md` history.
Source provenance is `../verification/source_snapshot_20261117.json`. Legacy fidelity limitations
remain explicit while the reusable components are extracted.

Request 49: [Pattern / Motion source decision](sequencers/PATTERN_MOTION_SOURCE_STUDY.md)
applies the full study to a 64-step performance sequencer. The engine and UI are
first-party; incomplete firmware documentation prevented upstream code adoption.

## Artist patch studies

[Rich and Roach patch strategies](instruments/RICH_ROACH_PATCH_STRATEGIES.md) traces played and sequenced lines, rhythmic delay, the track tunings on *Neurogenesis*, Roach’s live Part 3–4 transition and Rich’s cloud oscillator.
