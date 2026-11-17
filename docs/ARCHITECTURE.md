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

## Sequencer and effect interfaces

Request 38 pauses further implementation for the expanded source study in
[SEQUENCER_MODELS_RESEARCH.md](SEQUENCER_MODELS_RESEARCH.md). The resulting
[UI/module proposal](SEQUENCER_UI_ARCHITECTURE.md) recommends a shared configurable
Pattern module, distinct live-process/arp/conductor modules, and explicit capture
between incompatible native states. Views, traversal options and pattern generators
are not automatically separate module types. Definitions and playback-instance
state should be separate; control lanes retain their own types and units. Existing
timing code is provisional until assessed against these requirements.

Requests 33–35 add requirements for independent sequencer/arp models and modular
delay/reverb. Detailed proposals and evidence: `SEQUENCERS_AND_ARPEGGIATORS.md`
and `OPEN_SOURCE_DELAY_REVERB.md`. The local sample clock, grid divisions, bounded
event buffers and note-ownership ledger now exist in `timing.h/.cpp`, qualified
offline. They are not yet integrated into the audio runtime or app. The proposed
runtime connections below remain; 0.0.6 implements stereo ports and control-rate
parameter assignments:

- Shared transport includes tempo, beat/sample position, meter and discontinuities;
  individual modules retain explicit run/reset and clock-rate policies.
- Note events carry sample offsets and source/note identities; release ownership
  survives conditions, routing changes and module removal. UI timers do not clock notes.
- Different sequencer algorithms retain native state and contextual panels while
  sharing typed ports and stable parameter identities. Several may run together.
- Stereo effects can serve shared sends or inserts. Encapsulated feedback remains
  inside the processor; external feedback needs an explicit causal boundary.
- Delay buffers, reverb tails and frozen sound remain active after input silence.
  Tail continuity and captured-audio persistence need explicit contracts.

The complete Spectral engine now runs in the rack with embedded state, stereo
output and actual modulation exports. The note context remains monophonic; shared
transport, event cables, global voice groups and tail transfer remain to implement.

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
`Runtime` now adds mono/stereo audio/control execution with preallocated 24-frame buffers,
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

`RackDocument` persists the full graph as `optisynth.rack` version 3. Version 1/2
racks remain readable. Version 3 embeds full Spectral state and parameter routes;
version 2 added per-node sample paths/fingerprints, while
immutable decoded assets live outside GraphSpec. An explicit
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

The sample player receives an immutable bank prepared from reachable references.
Each player owns its playhead and articulation state. The native app's single
background loader reads/decodes assets and builds the candidate runtime; generation
tokens reject superseded requests. UI polling publishes only the current result.
Routine cached graph edits without whole engines prepare on the UI thread, never
the audio thread. Reachable Spectral engines always prepare on the same worker,
reusing compatible immutable tables. `SPECTRAL_ENGINE_INTEGRATION.md` records the
instance/memory boundary, snapshot voice policy and control-rate matrix mapping.
Sample-free graphs keep their existing parameter/schema behaviour. Full contract
and resource bounds: `SAMPLE_PLAYER.md`. This does not yet add event cables or
per-voice sample triggering; the current monophonic note context is retained.

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

## Proposed preset and direct-patching workflow — request 27

Use one saved routing graph for both playable instrument presets and direct
patching. A preset instantiates explicit nodes, values and connections; cables
show those same connections. Every preset remains editable, and every hand-built
rack can be saved as a preset. Avoid hidden preset wiring that disagrees with the
visible graph. The future agent API should edit this same document using stable
node/port/parameter identities.

Proposed interaction layers:

- Start with a prewired instrument template: subtractive voice, modal percussion,
  sample into resonator, or layered sources. Offer useful macro controls later.
- Drag modules from a browser, rearrange panels, replace compatible modules and
  remove them with undo. Moving a panel changes presentation only, not sound.
- Drag typed audio/control cables between sockets; highlight valid targets, show
  the selected module's connections clearly, and allow cable hiding/dimming.
  Keep click menus as an accessible alternative. Event cables require runtime work.
- Preview insert/remove consequences. A simple one-input/one-output audio processor
  may offer an explicit bypass/reconnect operation. Branches, mixers, modulators
  and output removal need clear disconnection/output choices rather than guessing.
- Save the complete instrument first; reusable groups of modules can follow with
  explicit boundary ports. Parameter-only variants must state that routing is retained.

Implementation sequence for this interaction work: undoable graph edits and safe
removal, visible draggable cables, module drag/reorder and compatible replacement,
then template browsing and reusable groups. This proposal does not silently cancel
the sample-player milestone. Prototype acceptance: preset routes match the displayed
cables; drag editing and menu editing produce the same document; undo restores both
sound settings and wiring; panel rearrangement preserves rendered sound; save/recall
preserves the graph; invalid routes leave the playing patch intact. Preparation
stays off the audio thread, and only reachable DSP is constructed regardless of UI.

Current 0.0.4 already has starter graphs, full-rack save/load, Add/Remove buttons,
module selectors and socket routing menus. Removal of active dependencies can be
rejected until rerouted. Cable drawing/dragging, panel rearrangement, undo and group
presets are proposed additions, not current features.

Interaction references checked 17 November: VCV's [Getting Started manual](https://vcvrack.com/manual/GettingStarted)
documents dragging cables/plugs and moving modules; its [Menu Bar manual](https://vcvrack.com/manual/MenuBar)
documents saved template patches, undo/redo and cable opacity. These support the
familiar interaction vocabulary. Optisynth retains its explicit audio/control types;
VCV's any-output-to-any-input voltage model is not the current Optisynth contract.
No VCV code was loaded, built or adopted for this design note.
