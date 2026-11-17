# Mutable DSP and contextual panels — 17 November 2026

Use original digital DSP, rather than reconstructing algorithms from hardware.
Analogue-only modules are a separate circuit-modelling research track. Plaits'
AnalogBassDrum already models pulse shaping, diode response, feedback and a
resonator; another circuit simulation is not a prerequisite to using it.

## Evidence and review boundary

- Eurorack revision `f4cfa7697095a28d292bff758beb51deb49b5a86`.
- Its stmlib submodule `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.
- Read root READMEs and stmlib LICENSE. Recursive documentation index has no
  Plaits-specific README/changelog/release notes. Other instruments' firmware,
  hardware, programming tools and build scripts are not adopted or executed.
- Read the [Plaits manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/plaits/manual/)
  and original panel image; prior research read Rings' manual and the VCV Rings
  adapter's rate/block boundary. VCV code/artwork is reference only.
- Read selected ModalVoice/Resonator and AnalogBassDrum source, sine oscillator,
  stmlib filter/DSP/units/interpolator/cosine/rsqrt/random helpers and dust source.
  Selected resource arrays are inert numbers: validate count, finite values,
  bounds and provenance before extracting. Do not include unrelated wavetables.
- The selected source files retain MIT notices. Product panels use our own names
  and drawings; provenance identifies upstream algorithms without implying endorsement.

## Mechanism and limits

First combination: modal percussion or analogue-style bass drum -> optional
Spectral filter effect -> optional VCA, with a separate Spectral envelope feeding
amplitude and/or filter cutoff. AHDSFR and a simpler ADSR configuration share the
existing envelope implementation; LP/HP/BP/notch start from our Spectral FilterFx.
These filters are independent behavioural fits, not LinPlug source. Spectral's
precomputed oscillator spectral filter is a different operation.

The initial playing graph is monophonic. The generic planner retains fan-out and
multiple-source support; voice allocation and a general cable editor follow this
slice. All construction and resource preparation stay off the audio thread.
Prepared output/control buffers are reused, and dormant nodes are not constructed.
Digital Mutable DSP runs at 48 kHz; the standalone wrapper must preserve this
rate through a bounded output conversion or explicitly report unsupported rates.
Never silently reinterpret the fixed-rate drum timing at another rate.

Module identity determines the panel descriptor, control units/ranges, layout and
ports. Modal controls describe material/brightness/decay; bass-drum controls
describe tone/decay/attack FM/self FM. Original panel hierarchy (large pitch and
timbre controls, smaller modulation controls, bottom ports) guides the layout.
Changing a module must actually replace its DSP and panel together. Saved state
uses stable module/parameter IDs, not screen position. Keep natural resonator tails
unless amplitude-envelope routing is selected.

Success means audible deterministic renders from both sources; distinctive panels
bound to the same parameter schema as DSP; working envelope/filter routing and
saved-state recall; finite bounded output at parameter extremes; no construction
or allocation in graph render; retained legacy tests. UI validation must inspect
rendered native panels. Offline tests do not establish hardware identity or Live
qualification.

## Side effects and dependencies before use

Selected Mutable DSP writes only caller-owned buffers and private filter/oscillator
state. No files, devices, sockets, MIDI ports, installers or firmware access.
ModalVoice's random sustain path is not enabled in this slice; the trigger path
is deterministic. Invalid parameter ranges could cause table overreads or unstable
output; clamp values and test boundary cases with memory/UB sanitizers.

Existing JUCE 8.0.8 review: `../documents/claude_code_tooling.md` in the workspace.
Re-read README, licence, current release notes/breaking changes and audio component
source for this wrapper. JUCE ResamplingAudioSource can resize and lock in its
callback; do not adopt it under an allocation-free runtime claim. Native controls
and AudioAppComponent use the established framework; no browser, sockets or plugin
scanning. Standalone audio opens the selected system output only (zero inputs),
and any hardware MIDI input must be explicitly selected. Save/load writes only
the selected patch file. Projucer writes local generated files plus its settings
and logs; Xcode writes build products. No install or Live action. Disable by
quitting the app; remove its local build bundle to uninstall. No new SDK download.
