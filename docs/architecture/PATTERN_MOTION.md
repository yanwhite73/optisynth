# Pattern / Motion — 0.0.8

Request 49 expands the existing Pattern module into a 64-step performance
sequencer. Euclidean and Stage keep their distinct models. This is a monophonic
note source with two independently looping control lanes, not a piano roll or
an eight-track PER|FORMER firmware port.

## Play and edit

Choose **Motion / Night current** or **Motion / Glass tide** in Starter patches.
Open the Pattern card's **performance editor**. Its Play button opens audio on
user click and starts the shared transport. Stop releases notes; Restart returns
to beat zero. Caps Lock typing works in the performance window too. The starter
has keyboard transposition enabled: C3 / MIDI 48 is its untransposed root.

The editor shows sixteen columns at a time, with four page buttons. Pitch,
velocity, gate, chance, ratchets, CV 1 and CV 2 can be drawn directly. Pitch drawing
covers MIDI 36–84; the selected-pitch field covers the full 0–127 range.
Shift-click selects a range; right-click toggles a note. The selected-step
inspector edits condition, pitch, gates and protection across the selection.
The amber marks show the actual gate and lane playheads, independently of editing
selection. Gate first/last and lane lengths are visible together.

Copy/paste retains all ten step fields, including conditions and protection.
One selected destination pastes consecutive steps; a larger selection repeats
the clipboard across that selection. Shift moves selected step data cyclically;
Rotation changes playback addressing without rewriting data. Duplicate copies
the active gate loop into following steps and extends it, up to 64.

Undo/redo groups one drawing or slider gesture into one operation (64 history
entries per open editor). Store/Recall snapshot temporarily captures the whole
Pattern state. These histories and the snapshot are editor-local; **Save** in
the rack stores the actual complete musical state.

New melody respects scale/root, generator base/range and protected steps.
Euclidean gates uses density over the active gate loop; New CV curves changes
only CV values. The three operations are separate, deterministic for the displayed
seed, undoable, and advance the seed. Protection applies to generators; deliberate
manual editing, copy/paste and shifts still work on protected steps.

## Playback contract

- Six traversals: forward, backward, pendulum (repeat endpoints), ping-pong
  (exclude repeated endpoints), random and wrapping random walk.
- Gate loop uses first/last. Pitch, velocity and both CV lanes each have a length;
  zero follows gate traversal/rotation, a nonzero length advances forward from
  step 1 on the common grid. This creates evolving combinations without several
  unrelated UI clocks. CV outputs update even on rests or failed conditions.
- Each step stores pitch, velocity, on, gate multiplier, chance, 1–8 ratchets,
  condition, bipolar CV 1/CV 2 and generator protection.
- Conditions: Always, Fill, Not fill, Previous, Not previous, First/Not first,
  1:2, 2:2, 1:3, 2:3, 3:3, 1:4, 2:4, 4:4. Previous means the previous evaluated
  **condition**, not whether a note sounded. Chance failure does not change that
  condition state. Always/Previous/Not previous preserve it.
- Non-random pass count follows absolute grid position and traversal period;
  random modes count local traversed steps. Restart/seek resets local random and
  previous-condition state. Seed changes take effect on the next reset.
- Swing delays odd grid positions by up to 45% of one step. Ratchets fit inside
  the remaining step. Step gate multiplies the global gate scale, separately
  for each ratchet. Duration is measured in beats, including tempo changes.
- Transposition and seven 12-TET scale choices affect emitted notes, not stored
  pitches. Quantization chooses the nearest note, downward on ties. Keyboard
  transposition latches the last held key relative to MIDI 48. This is not a
  held-chord arpeggiator or microtuning implementation.
- CV ports are bipolar normalized controls. Glide is linear at one normalized
  unit per selected second, with zero meaning immediate. Values hold at stop.
  Matrix destinations use their existing parameter range conversion and
  24-sample control rate; this is not audio-rate parameter modulation.

Stop, mute, clock discontinuity and loop/mode changes cancel pending ratchets and
release owned notes. Output note events retain exact sample offsets. All storage
is bounded and prepared off-thread; no render allocation, string lookup or lock.
A completed multi-cell edit publishes an even atomic revision. Render keeps the
previous coherent values if the UI is inside a transaction and never waits.
Old Pattern parameter IDs/defaults remain valid: old 16-step racks retain their
notes and timing. Rack v4 persists all new parameters and routing; transient
play state/history is not saved.

## Musical starters and boundaries

Night current and Glass tide use different registers/loops at 96 and 112 BPM.
The two CV lanes are already routed to the complete Spectral engine's filter
cutoff and wave mix. Its internal stereo delay feeds reverb; both presets are
original project patches. Delay times are stored in milliseconds for the starter
tempo, so changing transport tempo does not retime them automatically.

Remaining work includes chord/polyphonic steps, ties across different pitches,
MIDI recording, queued pattern/song banks, inter-module harmonic following,
reusable standalone FX and an independent held-note arp. The separate research
families remain the roadmap; the Pattern editor is not a promise that all of them
share one state model. No modular VST3 packaging or Live check is part of 0.0.8.

## Qualification status for this build

221 modular assertions pass normally and with AddressSanitizer/UBSan. The two
Motion starters also pass the separate headless `test_motion_presets.cpp` check:
12-second stereo renders, finite output and exact JSON/audio recall. The native
editor code compiles and the app signature verifies. Its widget checks and
screenshots remain **unexecuted**: desktop startup aborts in this session before
the test entry point. See STATUS and the build receipt for the precise boundary.
