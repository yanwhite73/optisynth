# Open-source sequencer models and methods

Research collected 17 November 2026 for request 38. **Research only: no sequencer
implementation, third-party execution, installation or app rebuild in this task.**
The earlier clock prototype is provisional. The proposed product design is in
[SEQUENCER_UI_ARCHITECTURE.md](../../ui/SEQUENCER_UI_ARCHITECTURE.md).

## Finding

Recommend a hybrid: a configurable Pattern sequencer for closely related stored
patterns, plus independently loadable modules for different musical processes.
Share timing, routing, capture, editing components and state infrastructure;
preserve each process's native state and characteristic interaction. This is our
synthesis of the evidence below, not an upstream architecture we have adopted.

The particularly useful references are PER|FORMER for typed tracks and generators,
Kria for independent parameter lanes, Stochas for probability semantics, Bespoke
for an actual modular software environment, Marbles for repeatable evolving
randomness, and Seq for composition above individual patterns. VCV's smaller
sequencers supply useful address, clock and signal-switching primitives.

## Evidence and collection method

Fourteen repository inventories were pinned. Thirteen contain source candidates;
the current Stellare repository is a negative finding, not a usable source port.
Selected original source, README/licence files, plaintext manuals, six HTML manual
snapshots and three UI reference images are saved under
`../../references/sequencer_research_20261117`. These reference copies are git-ignored.
[sequencer_research_sources_20261117.json](sequencer_research_sources_20261117.json)
records exact commits, URLs, file hashes, sizes and inspection scope. A downloaded
file is not automatically marked reviewed. The cards below identify the actual
source sections supporting the conclusions.

Manual pages can lag firmware: PER|FORMER's inspected manual identifies 0.1.40;
Stochas says its older commercial manual is mostly applicable, not fully current.
Code observations are tied to the pinned files. Tidal's GitHub README says active
development moved to [Codeberg](https://codeberg.org/uzu/tidal); its collected
GitHub snapshot is historical architectural evidence, not a claim of latest code.
No candidate was benchmarked or auditioned here.

This is **selected source research, not the complete pre-adoption review** required
by workspace AGENTS.md. Before importing/building any candidate, read its full
available documentation/release history and the chosen dependency closure; record
side effects, external ports, licensing, installation/removal and failure risks.
No imported source, firmware build scripts or external MIDI/network clients ran.

## Taxonomy: separate the stored model from operations on it

| Layer | What it means | Examples | Product consequence |
| --- | --- | --- | --- |
| Representation | What the user saves and edits | Note events, step cells, curves, stage lists, random memory, graph | Defines editor and native state |
| View | Another presentation of the same state | Piano roll, drum rows, faders, parameter lanes | Usually one module with multiple views |
| Traversal | Which stored item is read next | Forward, reverse, pendulum, random walk, external address | Usually an option, not a new sequencer |
| Generator | Creates or changes stored data | Euclidean fill, constrained phrase generation | Preview/apply/undo without replacing playback engine |
| Live process | Makes decisions as time or inputs arrive | Locked random memory, interacting counters | Usually needs its own runtime state and module |
| Note transformation | Processes incoming notes | Arp, transpose, quantise, note chance | A note-effect module with input/release ownership |
| Composition | Chooses/combines other patterns | Series, parallel, branching, scene launch | A conductor above pattern modules |

Euclidean rhythm spans two layers legitimately: it can fill a fixed pattern, or
remain a live generator whose hit count/rotation is modulated. The first must not
silently overwrite hand edits; the second needs visible generator state. A circular
view alone is not a different algorithm.

## Model families

| Family | State and musical method | Primary source evidence | Natural editor / decision |
| --- | --- | --- | --- |
| Fixed step/CV | Values and gate states at indexed positions | SEQ 3, Foundry | Step grid/faders; Pattern core |
| Polyphonic note/drum grid | Several notes per time column, durations and velocity | Stochas; local Optiseq | Piano roll/drum rows; shared Pattern state with typed lanes |
| Independent parameter lanes | Pitch, gate, octave, duration each have loop/rate/phase | Kria `kria_track`, `clock_kria_note` | Lane loops and separate playheads; Pattern capability |
| Addressed table | Clock and/or control signal selects stored values | Bogaudio `nextStep` | Read-position indicator and clocked/immediate selection; Pattern traversal |
| Variable-duration stages | Stage persists for N pulses; once/repeat/hold/rest behaviour | Bespoke M185 `StepBy`, `FindNextStep` | Stage cards with pulse counters; separate Stage module initially |
| Probabilistic grid | Independent firing or weighted selection, with dependencies | Stochas `getRandomSingle`, `getRandomMulti`, `isMandatory` | Probability overlay and condition inspector in Pattern |
| Evenly distributed rhythm | Hits over a cycle, rotation, independent lane lengths | PER\|FORMER `EuclideanGenerator::update`; Bespoke Euclidean rings | Pattern fill plus independent live Euclidean module |
| Topographic rhythm | Interpolated rhythm tables, density thresholds, controlled perturbation | Grids `ReadDrumMap`, `EvaluateDrums` | XY map and per-voice density; separate Rhythm Map module |
| Evolving random memory | New/replayed/reordered decisions, distribution and quantisation | Marbles `RandomSequence::NextValue` | Memory strip, lock/mutation/distribution; separate module |
| Counter/rule system | Countdown expiry modifies or resets other rows | Meadowphysics `clock_mp` | Counter rows and relation matrix; separate module |
| Curve/control sequence | Time-varying continuous value with segment shapes | PER\|FORMER `CurveTrackEngine`; QMidiArp LFO | Curve editor; typed Pattern lane, with control-only module preset |
| Held-note arpeggiator | Relative note indices resolve against a live chord buffer | QMidiArp manual and `MidiArp`; Bespoke Arpeggiator | Held-note display and order/rhythm; separate note-effect module |
| Hierarchical phrase/arrangement | Reusable motifs combined serially, in parallel or conditionally | Seq `Motif`, `SeriesClip`, `ParallelClip`, `AutomatonClip` | Phrase list/timeline/branch graph; separate conductor |
| Procedural pattern language | Operators or time-query functions generate events | Orca `parse/operate`; Tidal `Pattern` | Dedicated advanced editor; defer from first delivery |
| Sequential signal switch | Read order selects an external signal path | Befaco Muxlicer | Switching diagram and steps; routing utility, not just notes |

Different loop lengths at the same tick rate produce **polymetric** relationships;
different subdivisions over a shared span produce **polyrhythmic** relationships.
Both require visible lane phase. Neither is adequately described by one global
“number of steps” control.

### Randomness needs explicit semantics

| Method | Meaning to the musician | Required distinction |
| --- | --- | --- |
| Gate chance | This attack may be omitted | Never omit its required release |
| Weighted choice | Select one candidate using relative weights | Weights are not independent firing percentages |
| Conditional trigger | Depend on cycle count, fill or another event outcome | Explain which cycle/result is consulted |
| Parameter update chance | Keep the last value when update fails | Different from suppressing a note (Kria) |
| Random traversal | Read a different existing step | Does not necessarily change stored notes |
| Random walk | Move locally, possibly stay | Different from uniform random address |
| Memory mutation | Replace some remembered decisions | Save memory as well as seed (Marbles) |
| Timing/velocity variation | Perturb an otherwise selected event | Bound timing; preserve note identity and order |

These labels are proposed UI language. Stochas's mono/poly implementations provide
concrete evidence for the first two; Kria and Marbles demonstrate why one generic
“random amount” knob would be misleading.

## Source cards

### PER|FORMER — shared shell, typed tracks, generation as editing

[Manual](https://westlicht.github.io/performer/manual/),
[source](https://github.com/westlicht/performer/tree/f0c2328eb4ec6e638352fb2b04edffea1017c251),
[licence](https://westlicht.github.io/performer/license/).

The manual separates Note, Curve and MIDI/CV tracks within one performance shell.
Per-track timing and pattern selection coexist with step conditions, retriggers
and live snapshots. This is a good model for shared navigation without pretending
all tracks contain the same kind of data.

Inspected `engine/generators/EuclideanGenerator.cpp`: `update()` constructs the
rhythm, sets the builder's length and writes values into it. This supports a
generator-as-editing-tool, distinct from a permanently running generator module.
`SequenceState.h` separates traversal state from sequence content;
`NoteTrackEngine.cpp` separately evaluates gate chance, conditions and retriggers;
`CurveTrackEngine.cpp` has shape evaluation and its own gate queue.

Software is **MIT**; hardware is **CC BY-NC-SA 4.0**. Firmware timers, CV calibration
and board I/O are adaptation work, not part of an Optisynth module. No CPU claim
follows from it working on hardware. Best first candidate for a deeper source audit.

### VCV Fundamental SEQ 3 — the small baseline

[Source/manual](https://github.com/VCVRack/Fundamental/tree/ce3326407eb49ba188509b07912d58772b22e284).
Read `manual/SEQ3.md`: three CV rows share an eight-step traversal and trigger row;
external clock replaces internal clock. Run, reset and per-step outputs make the
sequencer a control source as well as a melody source. Clock-passthrough changes
trigger duration semantics. Good reference for an immediately understandable
compact panel, but insufficient for polyphonic events or independent lanes.

`src/SEQ3.cpp` is collected for follow-up; the manual supports these observations.
Source **GPLv3-or-later**; panel designs **CC BY-NC-ND 4.0**, branding separate.
Reuse the interaction lesson with original artwork. Rack voltage conventions and
its reset/cable timing cannot be transplanted without an explicit adapter.

### Impromptu Foundry/PhraseSeq — content, traversal and song are separate

[Manual](https://marcboule.github.io/ImpromptuModular/),
[sequencer implementation notes](https://marcboule.github.io/ImpromptuModular/README_SeqCode.html),
[source](https://github.com/MarcBoule/ImpromptuModular/tree/cf87c918875e502043cabe3deaa2e52adda7cecd).

Foundry distinguishes tracks, sequences, phrases and the pattern being edited from
the pattern being played. Inspected `FoundrySequencerKernel.cpp` persistence and
traversal sections: sequence run mode and song run mode are separate; Brownian
movement differs from uniform random selection. `PhraseSeqUtil.hpp` similarly
stores run mode separately from step attributes. This argues for traversal controls
inside Pattern and phrase ordering above it, not a new module for each direction.

Source **GPLv3-or-later**, with separate Rack excerpts noted in `LICENSE-ext.md`;
panels **CC BY-NC-ND 4.0**, some components **CC BY-NC 4.0**. The portable
[sequence clipboard format](https://marcboule.github.io/ImpromptuModular/clipboard-format.html)
is useful interchange evidence, but its CV units need conversion and it is not a
lossless description of every sequencer algorithm.

### Bogaudio ADDR-SEQ — address is independent of content

[Documentation/source](https://github.com/bogaudio/BogaudioModules/tree/656eaae458e045602dc974bae82e15a11e104958).
Read `addressable_sequence.cpp`: `nextStep()` combines clock advancement with an
external selection offset; selection may be sampled only on clock or updated
immediately. Wrap and negative-clock behaviour are explicit options, with saved
configuration. The stored table does not need a different format when its read
order changes.

Recommend addressed traversal in Pattern, with a separate reusable switch utility
when the operation selects *external signals* instead of stored values. Source
outside `lib` **GPLv3-or-later**; graphics **CC BY-SA 4.0**; dependencies require
their own review. Its 1 ms reset guard responds to Rack timing, not a requirement
for our timestamped event system.

### Stochas — probability is a musical data model

[Product description](https://stochas.org/stochas/),
[documentation status](https://stochas.org/documentation/),
[source](https://github.com/surge-synthesizer/stochas/tree/340a5b7d84d184e9c5d3487ef722e7962705bb0a).

Inspected `SequenceData.h` cell/chain representation and `StochaEngine.cpp` selection,
dependency and release handling. Mono selects a candidate by accumulated weights;
poly evaluates candidates and limits resulting polyphony. `isMandatory()` consults
remembered source-cell outcomes, including negation. Muting still advances random
decisions so the deterministic stream stays aligned; note release handling is
separate from choosing attacks.

The inspected screenshot uses one grid with Probability, Velocity, Chain and Shift
views. Links make dependencies visible, though many simultaneous links would need
selection/focus filtering in our UI. This supports a probability overlay in Pattern,
not another unrelated piano roll. Licence **GPLv3**; JUCE/plugin and MIDI wrappers
are separate from the selection algorithm. Full dependency review remains pending.

### Monome Kria — independent lanes with held values

[Manual](https://monome.org/docs/ansible/kria/),
[source](https://github.com/monome/ansible/tree/9796656cbbe9eb0227d380d555e31192f0cf32bf).

`ansible_grid.h` gives each parameter loop bounds, length and time multiplier.
In `clock_kria_note()`, a parameter updates only when its lane advances successfully;
otherwise its previous value persists. `clock_kria_track()` separately decides
triggers and repeated attacks. This is materially different from a single sequence
of fully specified note objects.

The manual and navigation image distinguish track selection, parameter pages and
loop/time/probability modifiers. Keep the lane concept, but expose several lane
loops at once on a desktop instead of copying hardware button chords. Pattern
should show whether pitch advances on clock or only when a trigger occurs.
Ansible includes a **GPLv2** licence; file/dependency qualifiers still need review.
USB grid, CV, I2C and firmware persistence are outside the prospective core.

### Monome Meadowphysics — counters and causal rules

[Manual](https://monome.org/docs/ansible/meadowphysics/), same Ansible source pin.
Read `clock_mp()`: rows count down at separate speeds; expiry applies rules to
destination counts/speeds, creates trigger/toggle changes and queues resets.
Resets are applied in a later pass. The explicit ordering is part of the method,
not merely a different drawing of a note pattern.

The inspected diagram places current position, countdown range, event and reset
on one row. For Optisynth, use counter rows plus a relation matrix showing what an
expiry affects. Keep this a separate module. Define same-tick rule ordering and
bound cascades before any implementation. Do not implement it by allowing arbitrary
zero-delay cycles in the audio graph. Same licence/adaptation limits as Kria.

### Mutable Marbles — random decisions with memory

[Manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/marbles/manual/),
[source](https://github.com/pichenettes/eurorack/tree/f4cfa7697095a28d292bff758beb51deb49b5a86/marbles).

Read `random/random_sequence.h` in full. `NextValue()` chooses between replacing
memory, replaying it in order, or jumping within it; the middle setting locks the
loop. `Clone()` explicitly preserves memory/history and repairs internal pointers.
There is replay state as well as a random stream: saving a seed alone cannot
reconstruct an arbitrary current loop. The arrays are bounded at 16 entries.

The manual's clock/random-trigger and voltage-distribution controls suggest a
distinct module with rhythm and value sections, memory lock and capture. This is
not equivalent to applying per-step chance to an existing melody. Relevant source
headers are **MIT**; stmlib, clocks, quantiser resources and rate assumptions still
need closure review. Original hardware branding/panels are not adopted.

### Mutable Grids — rhythm-map interpolation, with a separate Euclidean path

[Manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/grids/manual/),
[source](https://github.com/pichenettes/eurorack/tree/f4cfa7697095a28d292bff758beb51deb49b5a86/grids).

Read `pattern_generator.cc`: `ReadDrumMap()` interpolates four nearby map tables;
`EvaluateDrums()` compares each voice's level with a density threshold and derives
accents. Perturbation is chosen at pattern start. `EvaluateEuclidean()` instead
reads rhythm bit masks with independent lengths. These are different methods even
though the hardware offers both.

The map suggests an XY controller and per-voice density/accent controls, with a
live output preview. Keep it separate from the Euclidean editor. Source is
**GPLv3-or-later**, not MIT: the README explicitly separates AVR from STM32F code.
AVR program-memory access, EEPROM and static generator state require refactoring
for independent desktop instances. The pattern resources need a provenance audit;
do not describe the runtime table interpolation as live machine learning.

### Bespoke — distinct sequencers in a shared software environment

[Reference](https://www.bespokesynth.com/docs/index.html),
[source](https://github.com/BespokeSynth/BespokeSynth/tree/a0608a1acc704105c084af3b0fc3e63a6ed3e1b3).

The reference separates sequencers, note effects, pulse utilities and continuous
modulators. Inspected `IPulseReceiver.h`, `INoteReceiver.h` and `Transport.h`:
pulses and notes have separate contracts; pulses carry traversal/reset flags;
transport listeners describe intervals, offsets and lookahead. This is especially
relevant to our synth/environment rather than a standalone MIDI-only application.

`M185Sequencer.cpp` makes each stage last a configurable pulse count with
repeat/once/hold/rest gate behaviour; `FindNextStep()` bounds skipping zero-length
stages. Stage repetitions lengthen traversal; ratchets subdivide a duration.
`EuclideanSequencer.cpp` has a separate ring editor/generation path.

Source headers **GPLv3-or-later**. The full application can host plugins, run
Python and map MIDI/OSC; it was not launched. Adopt neither its wrapper nor its
buffer sizes without an independent review and workload measurements.

### QMidiArp — a live chord is an input, not a preset melody

[Project](https://qmidiarp.sourceforge.net/),
[source/manual](https://github.com/emuse/qmidiarp/tree/9ef4dfd0672c1023b9995da5d7a270dc2ab2f480).

Read the Arpeggiator sections of `man/qmidiarp.1` and source licence header.
Pattern note indices refer to the sorted incoming note buffer; octave, duration,
tempo and chord operations act on those indices. Continuous clock-follow,
keyboard restart on the grid and immediate keyboard-triggered playback are
distinct behaviours. The displayed shape is relative to held notes.

This justifies a standalone arp with held-note/latch visibility and explicit
restart policy. It may receive a chord sequence as well as a keyboard. QMidiArp
also separates step and LFO modules, sharing global state switching. Source
**GPLv2-or-later**. Its Qt/JACK/ALSA/LV2 environment is not a portable scheduler
to embed unchanged; those backends were not run.

### Seq — composition over reusable motifs

[Source and manual](https://github.com/eclab/seq/tree/932cb70fb9b4f68621e45f49d41c8331a4fa69ac).
Read the manual's hierarchy/editor sections and `Motif.java`'s state contract:
Motif is editable definition, Clip is a playback instance. One definition can
appear multiple times with independent playback state. Series/Parallel/Automaton
compose children instead of requiring every object to be another step grid.

Inspected `AutomatonClip.process()`: it detects revisited zero-time nodes and
limits active logical threads. These “threads” are playback paths, not evidence
that each motif creates an OS thread. Its Java collections/allocation patterns
are not an audio-callback implementation for us.

Licence **Apache 2.0**. Strong reference for an independent phrase conductor and
agent-readable composition tree, with explicit ownership of child playback and
note releases. Library dependencies and all current motif types remain to audit
before importing code. The UI's contextual motif display supports our focus editor.

### Orca and Tidal — optional procedural systems, distinct from each other

[Orca source](https://github.com/hundredrabbits/Orca/tree/223e45c84db14cb77fb1d31e29a3ad94652080d8)
is **MIT**. Read README operators and `orca.js`: it parses a spatial program and
executes eligible operators each frame. `parse()` constructs objects and `write()`
rebuilds strings, so this JavaScript loop is not suitable unchanged on our audio
callback. A dedicated spatial editor would be needed. Its application sends
MIDI/OSC/UDP and documents incoming UDP control; no such I/O was enabled here.

[Tidal snapshot](https://github.com/tidalcycles/Tidal/tree/fefb1c364af42be265e9f02477d6d2d8fb2ec2b9),
[cycle model](https://tidalcycles.org/docs/reference/cycles/),
[pattern composition](https://tidalcycles.org/docs/reference/pattern_structure/).
Inspected `Pattern.hs`: a pattern queries a time arc to return events, whose whole
duration and queried part are separate. This supports event-window scheduling and
compositional transforms without imposing one fixed step resolution. **GPLv3**;
the README includes explicit derivative-work guidance and points to Codeberg.
Treat it as a licensed reference, not a way to avoid licence obligations by
translating algorithms. Any future language runtime belongs outside the callback.

These systems could eventually share a bounded event output interface, but their
editors and saved programs are incompatible. Neither belongs in the first Pattern
mode selector merely because both can produce notes.

### Befaco Muxlicer — sequencing signal routes

[VCV port documentation/source](https://github.com/VCVRack/Befaco/tree/5774d41468be885902ea3bdd399f0872996b98a2).
Read `docs/Muxlicer.md`: the software explicitly adapts the hardware's
bidirectional switch to selectable 8-to-1 or 1-to-8 routing. It also changes
hardware gestures for mouse interaction. This is a useful precedent for familiar
panels with deliberate software adaptations. A sequential switch should be a
separate typed routing utility that can share traversal components with Pattern.
Code **GPLv3-or-later**; panel permission/component licences are separate. Switching
audio also raises click/crossfade issues that note sequencing does not solve.

### Negative finding: current Stellare Turing listing

[Current repository](https://github.com/stellare-modular/vcv-rack/tree/2bdfa2a736306dd7a6722c3b1565704a8027427d)
contains README/images rather than a current implementation and usable software
licence. The [VCV listing](https://library.vcvrack.com/StellareModular) says Freeware.
Do not count this as verified reusable open-source software. Original Music Thing
hardware openness does not establish a licence for this separate software port.
Marbles is the verified source candidate for the evolving-memory family in this
study. A binary shift-register implementation would still be a separate review.

## Local Optiseq bridge

The earlier [local source review](SEQUENCERS_AND_ARPEGGIATORS.md#what-local-optiseq-teaches-us)
covers `ableton/sequencer` and remains relevant: shared Notes/Steps/Faders/Lanes,
non-destructive performance changes, locked-note generation, seed streams, emitted
note capture and portable sequence state. Those are stronger reuse candidates for
our own Pattern workflow than duplicating a foreign desktop application's shell.

Preserve its distinction between source notes and the played result. A MIDI or
portable note export captures outcomes, not Kria lane rules, Marbles memory or a
branching composition. Its Max/Live scheduler and hard-coded timing assumptions
are not the Optisynth runtime. Existing pending Live/recall checks remain pending.

## Candidate priorities and remaining research

| Priority | Purpose | Next source review before code |
| --- | --- | --- |
| 1 | Pattern/curve/traversal contract | PER\|FORMER model/builders/queues plus local Optiseq data and conditions |
| 1 | Independent arp contract | QMidiArp note lifecycle and current first-party Spectral arp comparison |
| 2 | Independent lanes and probability | Kria advancement semantics; complete Stochas random/chain state |
| 2 | Distinct modular character | Marbles dependency closure; Bespoke stage semantics; live Euclidean instance |
| 3 | Rhythm map and rule networks | Grids resources/static state; Meadowphysics ordering and bounded cascades |
| 3 | Arrangement and agents | Seq motif/clip persistence and child-instance ownership |
| Later | Procedural editors / routing utilities | Current Tidal upstream, Orca runtime isolation, Muxlicer audio switching |

No performance rankings are justified yet. Tracker interfaces, cellular automata,
learned Markov melody models and constraint-solving harmonisers have not received
source-level coverage in this pass. They remain explicit gaps rather than extra
“supported modes”. The present set is sufficient to choose the shared/separate
architecture without claiming an exhaustive catalogue of open-source sequencing.
