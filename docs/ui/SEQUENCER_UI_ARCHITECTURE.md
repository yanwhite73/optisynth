# Sequencer modes, shared UI and module boundaries

Proposal from request 38, 17 November 2026. Evidence and exact source revisions:
[SEQUENCER_MODELS_RESEARCH.md](../research/sequencers/SEQUENCER_MODELS_RESEARCH.md). **No implementation
in this research task.** Names below are working product names, not adopted
third-party module names or a promise of the next release's full scope.

## Recommendation

Build one flexible **Pattern** module for stored musical material, alongside a
small family of independently loadable specialist modules. Share the timing and
editing framework, not one universal pattern structure. Any family can have
multiple instances and can feed other modules through typed ports.

PER|FORMER shows how a common shell can contain distinct track types; Stochas
shows overlays on one grid; Kria demonstrates independent parameter lanes. Marbles
and Meadowphysics preserve state that cannot be represented faithfully by ordinary
note cells. Bespoke provides the closest environment precedent. Seq provides the
composition layer. These comparisons support a hybrid over either a giant mode
selector or a separate implementation for every feature.

## Proposed product surfaces

| Module / surface | Modes or capabilities inside it | Shared UI | Unique UI and reason for boundary |
| --- | --- | --- | --- |
| **Pattern** | Notes/chords, drums/gates, control lanes; step/piano-roll/fader views; independent lane loops; probability/conditions; traversal | Track list, time grid, selection, loop braces, inspector, history | Typed lane editor; these are related stored patterns, not separate instruments |
| **Euclidean Rhythm** | Several live hit/length/rotation lanes, accent and per-lane rate | Clock bar, lanes, routing, output preview | Rings or linear rhythm strips; live generator parameters remain modulatable |
| **Stage** | Pulse count per stage; once/repeat/hold/rest; direction/address | Faders, scale, clock, selected-stage inspector | Stage cards show dwell and sub-position; fixed step cells conceal this state |
| **Memory** | Fresh, locked and evolving decisions; rhythm/value sections | Clock, seed/capture, scale/quantisation, port controls | Remembered loop, write/read position, distribution and mutation; distinct runtime state |
| **Rhythm Map** | XY pattern morph, density, accents, variation | Rhythm preview, per-voice output routing, clock | Map and density controls express interpolation rather than hit distribution |
| **Counters** | Countdown rows, ranges, expiry rules and relationships | Clock, mute, event preview, selection | Counter display plus relation matrix; causal rules need their own editor |
| **Arp** | Note order, octave span, rhythm, latch and restart policies | Clock, rhythm strip, gate/velocity controls | Held-note display and relative note indices; consumes and releases incoming notes |
| **Conductor** | Phrase chain, parallel parts, scene launch, optional branching | Pattern browser, names, history, launch quantisation | Child-instance list/timeline; advanced branch graph only where needed |
| **Procedural** (later) | Pattern expression or spatial program, separately versioned | Transport, capture, diagnostics | Dedicated text/spatial editor; no forced conversion to step cells |
| **Sequential Switch** (later) | Typed input selection/demultiplexing | Traversal widget, clock, address | Signal-path diagram; routing audio/control is distinct from generating notes |

A control-only preset can open Pattern directly in its curve view and advertise
only control/gate outputs. That gives it a compact “Control Sequence” entry in the
module browser while reusing implementation and state. It need not carry an unused
note engine. By contrast, a Memory instance must retain its own history even if
another instance uses the same settings.

Euclidean should be available **both** as a Pattern fill operation and as a live
module using a shared rhythm algorithm. Fill previews a result and applies it
once with undo. The live module keeps length/hits/rotation authoritative; hand
editing its emitted result means capturing into Pattern, not silently changing
what its knobs mean.

## Pattern: configurable without hiding the model

Use a track/typed-lane document, with note-event storage where appropriate. Do not
reduce arbitrary notes to one pitch per sixteenth. Preserve chords, overlapping
durations, ties, rests and microtiming. Drums use named destinations; controls carry
units/ranges/interpolation. A drum row and a piano-roll row can display the same
note events where mappings are explicit. Control curves are a sibling typed lane,
not fake MIDI notes.

Offer three clear *starting configurations*: Melody, Drums and Control. They are
templates, not destructive mode changes. Views can change freely without rewriting
data. Add lane capabilities progressively: velocity, duration, gate probability,
cycle condition, ratchets, then independently looping pitch or control lanes.
Enable only the selected features in the panel and runtime.

Expose four distinct operations:

| User intention | UI action | Saved effect |
| --- | --- | --- |
| See the same music differently | View: Steps / Notes / Lanes | Editor state only |
| Read the same values differently | Traversal: forward/reverse/pendulum/random walk/address | Playback policy, original data retained |
| Create editable material | Generate → preview → Apply | Versioned edit to selected lanes; undo restores prior material |
| Let a process keep creating events | Add a specialist generator | Independent module and native state |

Weighted single-choice and independent polyphonic chance can share the grid, but
must have explicit labels. Changing between them changes playback semantics; keep
the original values and show their new interpretation. Do not display relative
weights as if they were guaranteed firing percentages. Conditions inspect actual
outcomes through an explainable “why this event fired” view.

Independent lanes require visible loop bounds, rate and phase for *each* lane.
Example: 5 pitch steps, 7 velocity steps and 16 gates cannot be represented by one
moving vertical cursor. Default to linked lanes; unlink deliberately. Define whether
pitch advances on every clock, only on gates, or from a separate clock. Make reset
scope explicit: this lane, this track, this module or the transport group.

## What should be shared visually

Keep the approved rack/focus pattern. The compact rack panel has characteristic
performance controls and a useful live preview. Selecting Focus opens the editor
appropriate to that module. Changing a filter should not change the sequencer's
editor state; multiple sequencers retain independent focus, selection and zoom.

| Shared component | Applicable modules | Important qualification |
| --- | --- | --- |
| Header: name, preset, run/mute, clock source, rate, phase/reset | Clocked modules | Clock-derived modules show inherited timing; no fake local tempo |
| Typed input/output badges and destination list | All | Note, gate/pulse, control, pattern-launch and audio remain distinct |
| Selection, undo/redo, duplicate, copy/paste | Editable modules | Paste only compatible data; report conversion loss |
| Lane list, mute/solo, loop braces, grid and playheads | Pattern, Euclidean, parts of Stage/Control | Reuse widgets without forcing identical storage |
| Pitch/scale/range selector | Pitched Pattern, Arp, Memory | Hide on unpitched gates and arbitrary control data |
| Value inspector and modulation-depth controls | Parameterised modules | Show only parameters supported by the loaded model |
| Seed, new variation, freeze/lock, capture | Stochastic/generative modules | Lock memory and lock selected notes are different actions |
| Edit position versus playback position, optional Follow | Pattern, Stage, Conductor | Editing a future phrase must not unexpectedly switch playback |
| Captured-output/event inspector | All event/control producers | Show emitted events and why; don't confuse preview with actual output |
| Browser, presets, favourite configurations | All | Save native model/version alongside portable captures |

The existing modulation matrix can target continuous parameters such as density,
rotation or probability. Structural edits such as adding lanes or changing graph
rules need prepared transactions. Clock pulses and note streams need event routes;
they cannot be treated as ordinary scalar matrix assignments. Warn about an
incompatible connection in context instead of accepting a route that has no effect.

## Visual evidence applied

Request 39 explicitly checks that screenshots and interfaces are being collected.
Current visual coverage is an initial set of three images, not yet a complete
cross-product UI survey. These originals were saved and viewed:

- [Stochas grid and dependency-chain screenshot](../references/sequencer_research_20261117/manuals/stochas.png).
- [Kria track/parameter/modifier navigation diagram](../references/sequencer_research_20261117/manuals/kria.png).
- [Meadowphysics countdown/range/reset diagram](../references/sequencer_research_20261117/manuals/meadowphysics.png).

The manual snapshots and source study add interaction evidence, but do not replace
screenshots of actual editing and performance states. Next visual collection:
Foundry/PhraseSeq panels; Bespoke Stage/Euclidean/Arp; PER|FORMER note/curve pages;
Marbles/Grids control panels; and Seq arrangement editors. For each, record visible
control hierarchy, editing gestures, playback feedback, navigation, modulation,
pattern switching and how exceptions/errors are explained. Mark static observations
separately from behaviour confirmed by manuals; no hands-on testing is claimed.

This visual comparison precedes our wireframes. Preserve the earlier Matriarch and
Pigments references for the overall rack/modulation interaction. New third-party
images remain reference assets with source attribution, not copied app artwork.

The collected **Stochas screenshot** shows probability/velocity/chain views of
one pitch grid. Use that economy, with selected-cell dependency links and an
inspector instead of displaying every link at once. Colour must be reinforced by
numbers, symbols and text. Do not copy its artwork.

The **Kria navigation diagram** separates track, parameter and modifier choices.
Retain those conceptual layers, but the larger desktop display can show multiple
lane loops at once; hardware shift-key combinations need not be reproduced.

The **Meadowphysics diagram** explains a row through countdown, event, range and
reset. A bank of note knobs would obscure its method. Use explicit counter state
and a rule matrix alongside it. These three inspected images are available in
`../references/sequencer_research_20261117/manuals` and linked by the source manifest.

## A concrete rule for modes versus modules

Use a shared mode when saved content, input/output types, playback ownership and
editing gestures are compatible, and switching can preserve the content. A new
view is not a new mode. A new traversal policy usually stays in the same module.

Use a separate module when it needs different input ownership, persistent process
state or an editor that explains a different musical operation. Also allow separate
instances whenever the musician may want both processes running together. Shared
widgets or algorithms do not require a single module identity.

| Comparison | Decision | Reason |
| --- | --- | --- |
| Steps ↔ piano roll ↔ faders | Views of Pattern | Same compatible notes/lanes |
| Forward ↔ reverse ↔ address | Pattern traversal options | Content survives; read policy changes |
| Independent gate chance ↔ weighted single choice | Explicit Pattern playback option | Same grid, different selection rule; preserve values and label semantics |
| Linked ↔ independent parameter loops | Pattern lane capability | Same lane values, explicit separate phase/state |
| Step sequence ↔ variable-duration stages | Separate Stage initially | Dwell/pulse semantics and editing differ; capture bridges them |
| Euclidean ↔ rhythm-map interpolation | Separate modules | Different authoritative state and musical control; useful simultaneously |
| Stored melody ↔ live arp | Separate modules | Arp owns a held-note input set and relative indexing |
| Gate probability ↔ evolving random memory | Separate modules | Independent firing does not preserve a mutable remembered loop |
| Pattern ↔ phrase arrangement | Separate conductor | A container controls child instances, not individual cells |

Stage and Pattern may later share more storage if a lossless editable mapping is
demonstrated. Do not promise that mapping simply because a finite performance can
be flattened to MIDI. Flattening can discard the reason notes were generated.

## State, interchange and agent access

Proposed common envelope: stable module/port/parameter IDs; model identifier and
schema version; native definition; playback/reset policy; random seed and evolving
memory where relevant; optional portable event/control capture; editor state.
Keep immutable definition separate from independent playback-instance state,
following the useful distinction in Seq. Do not serialize raw pointers or firmware
memory dumps.

Preset recall and session resume are separate policies: “restart from pattern
beginning” versus “resume saved memory/phase”. An arp should not invent physically
held keyboard notes after recall; a deliberately latched chord can be saved with
an explicit retrigger policy. A conductor referencing one phrase twice needs two
playback instances even if the definition is shared.

Give agents the same typed operations as the editor: describe capabilities,
read native state, edit lanes/steps, set generator parameters, preview, apply a
prepared change at a named boundary, and capture emitted output. Include revision
checks, stable lane/event IDs, units and bounded ranges. “New variation” changes a
seed/variation index; “Apply” with identical inputs remains reproducible. Keep
source, performance overlay and capture separate so agent edits can be undone.

Conversions are explicit and non-destructive: capture a chosen length of Memory
into Pattern, or render a Conductor passage to notes while retaining its source.
Report when a capture has truncated a longer cycle or omitted control/program
state. MIDI export alone does not preserve all module behaviour.

## Timing and performance implications

This is a proposed contract, not a measured implementation:

- Prepare only reachable modules and selected capabilities. One instance owns
  independent state; shared immutable definitions/resources need not be duplicated.
  An empty step is not an inactive sequencer: it may hold notes or affect later rules.
- Use shared transport with explicit meter, musical position and discontinuities;
  retain module clock/reset policies. Don't flatten all polymetric loops to their
  potentially enormous least-common-multiple pattern.
- Prepare stored/generated event windows off the callback; process immediate live
  inputs through bounded rules at event boundaries. Negative microtiming needs
  declared lookahead; unexpected input cannot be scheduled into the past.
- Separate event-generation rate, continuous-control evaluation and audio sample
  rate. Timestamped notes need sample-accurate delivery without polling every
  sequencer at every audio sample. Audio-rate modulation is a separate capability.
- UI reads decimated snapshots. Never schedule music from redraw timers. Compile
  text/graph programs and parse files off the callback; bound future event density,
  ratchets, graph visits, nested instances and queue memory.
- Stop, seek, removal, mode change and rerouting must release notes owned by that
  source. Muting can either preserve phase/random decisions or pause them, but
  the chosen policy must be visible and saved. Reproducibility must not depend on
  screen refresh or audio block size.
- Rule feedback inside Counters/Conductor needs documented causal ordering and a
  work budget. It does not justify removing the audio graph's cycle validation.

The 47 existing timing checks cover a useful isolated foundation. They do not
qualify independent polymetric lanes, live arps, dense ratchets, graph rules,
microtiming, host transport, or the eventual VST3 integration. Reassess that prototype
against this contract before connecting it to the runtime.

## Design checks before the next build

1. Make the Pattern lane/event definition and input/output units concrete, including
   chord ownership, control curves, independently clocked lanes and reset scope.
2. Complete the chosen source dependency/licence/side-effect audit; distinguish
   direct reuse, adaptation and independently specified first-party behaviour.
3. Make static rack/focus wireframes for Pattern, Arp, Stage and Memory to test the
   shared header/inspector against genuinely different editors. No running engine
   is needed to resolve these UI boundaries.
4. Specify five acceptance scenarios: a drum grid with conditional fill; 5/7/16
   independent lanes; a chord source into Arp; live Euclidean percussion alongside
   Spectral; locked evolving memory captured into an editable pattern.
5. Specify lifecycle/performance fixtures: overlapping same-pitch sources; stop,
   seek and reroute; varied block sizes and meters; recall of memory and conditions;
   bounded dense events; graph cycles and inactive-module cost.

When implementation resumes, Pattern plus Arp is the smallest useful first pair;
one specialist module should follow early to prove that the architecture supports
more than views of the same sequencer. Memory and Stage are particularly useful
tests. Rhythm Map, Counters and Conductor remain planned families, not requirements
to cram into the first release. This research milestone ends before coding them.
