# Optisynth architecture — foundation

## Intended instrument

Reusable generators, processors and modulators share a typed patch graph. Spectral,
sample, granular, virtual-analogue and physical-modelling components can be mixed
within one patch. An envelope may control several components; a shared effect may
receive multiple voices or layers. Modes inspired by particular instruments become
module combinations and presets, with measured fidelity tracked separately.

The core remains framework-independent C++17. VST3 and a standalone editor will
wrap it. A structured patch and offline renderer are the basis for agent access;
the editor, automation and agent interface must address the same stable identities.

## Scope and routing

- Audio, control and note/trigger connections have explicit types and units.
- A voice graph is replicated per active note. Global effects/modulators run once;
  voice outputs cross into that graph through an explicit mix boundary.
- One envelope can fan out to multiple destinations within its scope. Sharing an
  envelope per voice differs from imposing one global envelope on every note.
- Feedback requires an explicit causal delay or a separately designed solver.
  The first graph planner rejects cycles instead of guessing an execution order.
- Structural edits prepare a replacement plan off-thread. Scalar edits use stable
  resolved handles and appropriate smoothing. State retirement stays off-thread.

## Performance contract

Determine output reachability before constructing processors. Follow both audio
and modulation dependencies; unused modules remain inexpensive patch descriptions.
Prepare only reachable modules and their assets, in dependency order. Store that
order once; render does no graph search, string lookup, file access or allocation.

Share immutable spectra/tables and sample buffers, but keep per-voice phase,
envelopes and filter state separate. Bound node, voice, grain and buffer counts.
Apply oversampling only where the DSP needs it, with explicit conversion boundaries.
Do not unload a live processor merely because its input becomes silent: effect
tails, modulation clocks and delayed feedback must continue correctly.

Measurements must cover cold preparation, memory by active module, steady-state
CPU, tail CPU, dense polyphony and graph changes. The inherited engine's extreme
polyphony/sample-rate overruns mean that current reuse is not a performance guarantee.

## Spectral extraction

The inherited `Envelope`, `Effect`, `Spectrum` and `TableBank` have useful separate
interfaces. `Voice`, its modulation enums and `Patch` are tied to four Spectral
oscillators. Begin with reusable table sampling/phase processing and routing, while
keeping the legacy engine as a compatibility path and regression reference.

Spectral filtering is partly precomputed into waveform tables. It is not a general
audio input filter. Samples and granular audio need separate audio filters or
resonators; a future spectral audio processor would be a different algorithm.

## Incremental acceptance

1. Independent source snapshot and project-local research/request record.
2. Typed graph planning: invalid links and cycles rejected, unused modules never
   constructed, dependencies sorted, fan-out supported; reusable Spectral sampling
   extracted without changing legacy renders.
3. Connect that planner to generators, envelope/VCA/mixer and existing effects.
   Demonstrate two sources sharing an envelope and a shared effect in a saved patch.
4. Add sample playback and a modal percussion resonator; demonstrate both a sample
   exciting the model and a separate sample attack mixed alongside it.
5. Granular/virtual-analogue modules, voice management and a graph editor; integrate
   and qualify a distinct Optisynth VST3 plus the agent control interface.

These are successive deliverables. A planner or source refactor alone is not a
playable modular instrument, and an offline pass is not Live verification.

Milestones 1–2 are implemented at foundation level. The planner validates dormant
graph structure too, but never invokes dormant module factories. It traverses
control/event dependencies as well as audio. `PreparedGraph` owns module lifetimes;
`Runtime` now adds mono audio/control execution with preallocated 24-frame buffers,
resolved numeric connections and active module construction. Event cables are not
executed yet; one monophonic note context reaches source and envelope nodes.
The first extracted Spectral sampling primitives preserve existing audio exactly
on the three verified fixtures; this is regression evidence, not new fidelity.

## First playable runtime and UI

`panels()` supplies stable module/parameter IDs, labels, ranges and panel families.
Factories and the native editor consume the same schema. Selecting modal percussion
or bass drum changes both processor and panel; switching envelope/filter variants
does the same. Inactive envelopes remain editable patch descriptions, clearly
marked as unrouted; routing activates their DSP. The rack now exposes typed input
selectors, multiple instances and an audio output selector. Each node has independent
parameter values and DSP state; same-algorithm copies do not share knobs.

`RackDocument` persists the full graph as `optisynth.rack` version 1. An explicit
import path converts 0.0.2 `optisynth.instrument` patches. All nodes, including
dormant ones, have their saved parameters validated. A candidate graph is compiled
and prepared before publication; invalid edits retain the current graph. Only
audio outputs may be roots. Inputs expose same-type outputs, with cycles rejected
by the compiler. Knob changes update the saved node and its resolved atomic control
without rebuilding. Selecting the output can make a continuous oscillator audible
without a gate; envelope/VCA routing explicitly controls note articulation.

`Player` exchanges a prepared replacement through a single-producer/single-consumer
atomic mailbox. The callback crossfades old/new plans for 240 internal samples and
passes the retired pointer back for UI-thread deletion. Rapid UI changes replace
only an unconsumed pending plan. Scalar changes use atomic values and 5 ms smoothing.
No shared-pointer reference counting or graph ownership destruction in render.

Mutable DSP keeps a 48 kHz internal rate; `OutputConverter` uses precomputed
32-tap/256-phase windowed-sinc coefficients and bounded history. The wrapper adds
roughly 16 internal samples of conversion delay plus a 24-sample render quantum.
Output gain is smoothed and unexpected nonfinite/overrange output is contained.
This is host protection, not a recreation of hardware DAC/output electronics.

The standalone is native JUCE with no browser/network. It opens the default system
audio output only when Start audio is clicked. The UI provides typing/piano notes;
hardware MIDI, polyphony and an agent API remain unimplemented. Core render is
allocation/lock free in the tested paths; JUCE's outer device manager has its own
callback synchronization, so this is not a claim about every OS/framework layer.
