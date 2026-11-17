# Rack and routing — 17 November 2026

Next playable feature: multiple independent module instances in a native rack.
Use the existing typed graph compiler/runtime, contextual controls and prepared
plan handoff. Preserve 0.0.2 instrument patches through an explicit import path.
Sound-quality tuning remains deferred at Yan's request.

Each module instance has its own stable node ID and parameter state. Multiple
instances of the same algorithm must not share knob values or filter/voice state.
An output selector chooses the audio root. For each input, choose an output of
the same signal type; explicit mixer nodes combine audio, VCA nodes apply control.
Allow incomplete dormant modules while assembling a patch; activating an incomplete
or cyclic route must fail validation before replacing the audible graph.

UI: rack module list and add/remove controls; selected module's contextual panel;
visible connection rows and output destination. Show dormant modules as unrouted,
with clear validation feedback. Graph files use a new versioned JSON format with
nodes, typed endpoint identities, parameter values, connections and output roots.
No strings/JSON/graph construction or deletion in the audio callback.

Requests 18–19 bring original Mutable oscillator/filter code into this step.
Read the complete pinned VariableShapeOscillator and Oscillator headers, plus
their previously reviewed DSP/interpolator/polyBLEP dependencies. The algorithm
blends triangle/saw/pulse and supports hard sync with interpolated parameters and
BLEP corrections. Use its original normalised frequency limits and nonzero pulse
width bounds. Existing stmlib::Svf source provides a digital state-variable filter
with LP/BP/HP outputs; expose these under their own digital-filter names, with
upstream provenance in the panel description. This is not a circuit emulation of
the analogue Ripples module. No new circuit design or firmware tools are run.
Both routines operate only on supplied buffers/private state, with no device,
file, network or MIDI access. Bad bounds can destabilize audio; retain finite-
output/extreme-value checks. Remove the selected nodes to disable their DSP.

Add two useful modules from the already-read/pinned dependency closure: Plaits'
sine oscillator as a continuous source and its modal resonator with an explicit
external audio input. No new third-party code/download/install required. The
resonator supports a drum or another source as excitation; sample loading follows.
Retain upstream notices and the fixed 48 kHz rate. No VST or Live action in this step.

Acceptance: independently detuned copies of one source; source -> resonator ->
filter; one envelope controlling multiple VCAs/filter inputs; unused nodes remain
unconstructed; invalid edges/cycles leave current sound intact; old patches import
without audio changes; full graph recall reproduces audio; native snapshots and
UI actions verify add/remove/connect/disconnect and instance-specific controls.

## Implemented result

The 0.0.3 editor exposes 17 module types. Inputs use typed source dropdowns;
outputs use a separate audio-root selector. There is no drag-cable interaction yet.
Multiple copies of the same algorithm have independent node IDs, controls and DSP.
The full graph format is `optisynth.rack` version 1; 0.0.2 instrument import preserves
active sound. Native tests exercise the controls/routes and compare recalled audio.

Four starter patches cover VA/filter, modal percussion, drum/external resonator,
and two oscillators through a mixer and shared filter/envelope. New original DSP
sources are vendored with preserved notices; no upstream firmware scripts run.
Digital SVF names and descriptions explicitly distinguish them from Ripples.

All 74 modular checks pass normally and under ASan/UBSan. The app builds, native
checks pass, and four contextual/minimum-size captures were inspected. No hardware
audio or Live verification is claimed. Aliasing/performance tuning stays deferred.
