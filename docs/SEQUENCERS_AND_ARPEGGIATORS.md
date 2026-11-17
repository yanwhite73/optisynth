# Sequencers and independent arpeggiators

Research/design history, 17 November 2026. Requests 33–34 are retained verbatim in
`USER_REQUESTS.md`. Standalone 0.0.6 implements complete Spectral, stereo and shared
parameter routing; an isolated timing prototype followed.

**Request 38 supersedes the implementation order below.** Research is now expanded
in [SEQUENCER_MODELS_RESEARCH.md](SEQUENCER_MODELS_RESEARCH.md), with pinned source,
manual/licence evidence and a model/method taxonomy. The proposed hybrid of shared
Pattern modes and specialist modules is in
[SEQUENCER_UI_ARCHITECTURE.md](SEQUENCER_UI_ARCHITECTURE.md). No further building in
this research task. The earlier shortlist and timing contract remain historical
context, not a constraint on the revised design or an instruction to resume coding.

## Direction

Support different sequencer algorithms as independently loadable modules, with
their own contextual panels and several instances per patch. Share transport,
typed events and parameter destinations; do not force every algorithm into one
fixed step-pattern representation. A piano roll and fader view of the same notes
are useful editing views, but do not themselves constitute different models.

| Proposed model | Musical behaviour | Panel emphasis |
| --- | --- | --- |
| Step/CV | Explicit steps, pitch/gate/control rows, ties, ratchets and probability | Knobs/faders, active step, independent lane lengths |
| Phrase/generative | Motifs, constrained variations, editable notes and phrase chains | Optiseq-style Generate / Sequence / Perform / Save |
| Euclidean rhythm | Hits distributed around independently cycling lanes | Length, hits, rotation, accent and drum destinations |
| Evolving loop | Lock a repeating sequence, then allow controlled mutation | Loop length, mutation, lock and capture |
| Addressable sequence | Another signal selects the step or traversal position | Stored values plus visible read position |
| Independent arpeggiator | Held notes/chords become ordered rhythmic notes | Order, octaves, latch, gate, swing and rhythm |

Example: a phrase module supplies chords to an independent arp playing Spectral;
a Euclidean module triggers percussion; a seven-step control lane moves a
resonator's brightness. They share tempo while retaining separate cycle lengths.
Capture the emitted result when transferring between incompatible model types;
preserve the original model and settings alongside the capture.

## What VCV references teach us

- [VCV SEQ 3](https://vcvrack.com/Free#SEQ3) separates three control rows from
  triggers, with internal/external clock, run, reset and length. Its per-step
  outputs and chaining show why a sequencer should control timbre and structure
  as well as notes. These are interface lessons, not a proposal to adopt Rack's
  voltage units throughout Optisynth.
- [Impromptu Foundry and general concepts](https://marcboule.github.io/ImpromptuModular/#foundry)
  distinguish tracks, sequences and song phrases; independently clocked parts
  can have different lengths. Editing position and playback position are
  distinct. Forward, reverse, pendulum and stochastic traversal provide musical
  variation without rewriting stored notes. We should expose which pattern is
  playing and which is being edited, with explicit follow-playhead behaviour.
- [Bogaudio ADDR-SEQ](https://github.com/bogaudio/BogaudioModules/blob/master/README.md#addr-seq)
  combines clock advancement with an external selection signal. This supports
  treating stored step data and read order independently, including clocked
  sampling of the address rather than uncontrolled jumps between ticks.
- [Music Thing Turing Machine](https://www.musicthing.co.uk/Turing-Machine/)
  uses a shift register to produce evolving sequences that can lock into a loop.
  For our software version, saving seed, register contents and traversal state
  would make results recoverable. That persistence is our proposed addition,
  not a capability claimed for the original hardware.

[VCV's timing standard](https://vcvrack.com/manual/VoltageStandards#timing) and
[Impromptu's sequencer example](https://marcboule.github.io/ImpromptuModular/README_SeqCode.html)
explain reset/clock ordering problems caused by delayed patch paths. Our event
graph should define timestamp and simultaneous-event ordering explicitly. Do
not blindly copy Rack's one-millisecond post-reset clock suppression: Optisynth
does not currently have Rack's one-sample delay on every cable.

## What local Optiseq teaches us

Read-only inspection of `../../ableton/sequencer`: README, research/start-here
documents, current engineering-log/backlog entries, conditional-trigger design,
and relevant `src/pattern.js`, `core.js`, `v2_ui.js`, `workspace_adapter.js`,
`sequence_file.js` and `track_playback.py` sections. No private research evidence
was copied. Version headings lag the latest engineering log; this is a source
inspection, not a claim that every feature is verified in the installed device.

Useful existing patterns:

- Separate source notes from non-destructive performance changes. Notes, Steps,
  Faders and Lanes are views into shared musical state.
- Preserve loop bounds independently of zoom. Keep locked notes when generating
  variations. Named seeded random streams make musical decisions reproducible.
- Independent pitch, velocity, gate and chance lane lengths/rates create movement
  without requiring a new melody every bar.
- Prepare playback data ahead of time, then switch prepared banks at a defined
  musical boundary. Capture actual emitted notes, including probabilistic results.
- Keep portable musical state separate from Live IDs and UI implementation.
  `OptiseqSequence` v2 provides a concrete future import/export bridge.
- Conditions/probability affect new attacks; they must not suppress required note
  releases. Ratchets inherit the parent event's condition.

Carry over the musical model, not the Max scheduler or Live integration. The
current playback code contains 480 ticks/beat and 1,920 ticks/bar conventions;
Optisynth must carry the time signature instead of assuming every bar is 4/4.

Known lessons to preserve: the log records host save-state truncation at 32,767
characters, with a chunking fix awaiting actual Live recall verification. Test
our complete future VST3 save/restore path with large patterns; that Max limit is
not a VST3 limit. Euclidean Apply currently deterministically rebuilds the same
settings, including from its dice action: distinguish Apply from New Variation.
An explicit Turing model and independent start/stop are not completed Optiseq
features to claim as ready-made reuse. Backlog items also report harmony-choice
and performance-view issues; inspect before porting those paths.

## Shared event and clock contract — proposed

Transport carries sample position, tempo/beat position, time signature, play state
and seek/loop discontinuities. A module selects host-follow or local running,
clock division/multiplication, swing and explicit reset/resume rules. UI refresh
timers never schedule audio notes.

Note events carry a sample offset, source identity and note identity, plus pitch,
velocity and release. Clock/reset/gate and continuous modulation remain distinct
types. Define priority for same-sample reset, release and attack; define held-note
behaviour on stop, seek, removal, rerouting and queue overflow. Bound event storage,
ratchets and polyphony. Prepare generation and state snapshots off the callback;
playback consumes scheduled events without searching every note each sample.

All models share the connection contract, while native state remains versioned
and model-specific. Save seeds and evolving state where reproducibility matters.
UI and agent edits address the same stable IDs. Reachability includes note/control
edges, so a sequencer resting this beat is not discarded as unused.

The inherited `partials::Arpeggiator` already offers ordering, octave/rhythm/gate
logic and fixed storage. However, its output event has no sample offset and the
engine applies batches at a 16-sample control boundary. Reusing it unchanged would
not deliver sample-accurate modular event routing. Retain Spectral's internal arp
for preset compatibility; external arp chaining must be explicit.

[VCV portable sequence JSON](https://marcboule.github.io/ImpromptuModular/clipboard-format.html)
is a useful exchange reference: beat-based note timing, pitch, optional velocity
and probability. Its voltage-scaled pitch/velocity require explicit conversion.
Use a portable note subset alongside full native state; it cannot represent every
model's algorithm, modulation lanes and evolving register state.

## Earlier implementation order and acceptance — superseded by request 38

1. Preserve the complete-Spectral milestone; establish note/transport boundaries
   alongside its stereo/state integration rather than binding it only to keyboard.
2. Implement a simple step/control sequencer and independent arp to qualify those
   interfaces, routing and contextual UI.
3. Adapt Optiseq musical state/generation and portable sequence exchange, without
   embedding Max or Live dependencies.
4. Add Euclidean, addressable and evolving-loop models incrementally.

Acceptance must cover several instances with different cycle lengths; deterministic
timing across audio block sizes; non-4/4 meters; tempo changes/seek/loop/reset;
stop/removal without stuck notes; predictable overflow; seeded recall; captures;
large saved states; and no callback allocation. Existing modular playback still
has shared monophonic Note context: event cables and this scheduler are future work.

## Timing foundation — request 37, implementation contract

Implement and qualify this first-party foundation offline before connecting it to
Runtime/Player. The user-auditioned 0.0.6 application stays unchanged in this slice.
This is not yet a playable sequencer module or a VST3 transport adapter.

Evidence refreshed: [VCV timing](https://vcvrack.com/manual/VoltageStandards#timing)
explains its cable-delay reset workaround. Our typed events instead share explicit
sample offsets and ordering. [VST3 ProcessContext](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html)
separates sample position from quarter-note position and validity-tagged tempo,
meter and loop data. Hosts need not split a block at every loop jump. A future host
adapter must check those flags and reconcile discontinuities; do not infer host
support merely from having a local clock.

Concrete first slice:

- One local sample clock supplies immutable frames to all consumers. Its render
  sample counter is monotonic, even while stopped; quarter-note position freezes
  while stopped and may jump on seek/loop. Tempo changes preserve musical phase.
  Meter gives quarter notes per bar (7/8 is 3.5), including negative preroll.
  Start/stop/seek/loop generate explicit discontinuity flags and a revision.
- Grid clocks share those frames but retain independent divisions and traversal
  indices. Starting/seeking exactly on a grid emits that boundary once; otherwise
  wait for the next boundary. No invented 1 ms suppression. Swing, generation,
  ratchets and module-specific reset behaviours are subsequent work.
- An event block has a bounded 256-event array, sample offsets, source identity and
  source-local note identity. Same-offset ordering is reset, release, attack; ties
  use stable source/note IDs. Offsets must be inside the block. Invalid input or
  capacity overflow fails the entire affected block closed: discard its events,
  require all held notes on that bus to release, and reject further insertion
  until the next block. Never silently drop a release and keep attacking.
- A fixed 64-note ledger distinguishes same-pitch notes from different sources.
  Source reset/removal releases only that source; global reset/overflow releases
  the ledger. Unknown releases are harmless. This is note ownership, not an audio
  voice allocator or the implementation of polyphonic synthesis.

All methods called in playback use fixed storage, no allocation, locks, file or
network access. Configuration is validated; thread handoff remains the caller's
responsibility. Success: identical event samples across chunk sizes, tempo change
phase, negative positions, 3/4 and 7/8, seek/loop/start/stop ordering, overlapping
source ownership, overflow release and zero allocation; run under ASan/UBSan too.
Then integrate event ports and split rendering at event offsets, followed by the
first step/control sequencer and independent chord arpeggiator with contextual UI.

### Foundation result

Implemented in `src/modular/timing.h/.cpp`; `tests/test_timing.cpp` supplies 47
checks passing normally and under ASan/UBSan. Log: `build/timing-qualification.log`.
Clock commands reject invalid data without changing state. Grid sequences match
across sample grouping sizes 1, 17, 24, 64, 511 and 1024; a minute at 123.45 BPM
has less than one sample of boundary quantisation error. Noninteger loop lengths
retain fractional overshoot. Meter, negative preroll, tempo phase, start/stop,
seek and independent divisions are covered.

Event ordering, source-specific release/removal, duplicate identities, invalid
events and both event/held-note capacity failures are covered. A held-note ledger
overflow also suppresses subsequent attacks until the next valid block. Normal
clock/event/ownership playback performs zero counted heap allocations. Note IDs
must remain unique for the lifetime of an owned note; reuse across retired plans
must be isolated by the future event adapter or a new source identity.

This code is not yet linked to Runtime/Player or the app, and does not claim
audible sample-accurate playback. Next concrete change: replace the shared Note
context at explicit event inputs, preserve legacy keyboard fallback on unconnected
inputs, and split each receiving processor's render at event offsets. Qualify
release on removal/rerouting/stop before adding the step and arp panels. The app
binary, rack format and inherited Spectral arp remain at the auditioned 0.0.6 state.
