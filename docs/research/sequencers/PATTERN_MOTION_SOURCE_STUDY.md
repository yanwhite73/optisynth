# Pattern / Motion source decision — 19 November 2026

The full model/UX study remains the basis: PER|FORMER for traversal, conditional
notes and performance state; Kria for independently looping lanes; Stochas for
visible probability; Bespoke for a desktop lane editor; OXI for separating active
playback from editing and retaining performance state. Pattern is the shared
stored-data model; Stage, Euclidean, a future live-process sequencer, independent
arp and conductor retain their different semantics.

PER|FORMER is the best permissively licensed firmware candidate in this study.
Its source catalogue pins westlicht/performer at
`f0c2328eb4ec6e638352fb2b04edffea1017c251`. Its software licence is MIT; the hardware
licence is separate. README, MIT text, the complete archived official manual,
sequence state declaration and the relevant NoteTrackEngine gate/condition/
retrigger/note helper source were read. Existing P02 step-editing and P04 quick
access screenshots informed the desktop hierarchy, alongside the complete visual
atlas already collected. The upstream [changelog](https://github.com/westlicht/performer/blob/master/CHANGELOG.md)
was read through the web connector after local DNS failed. Its fixes reinforce
bounded first/last edits, safe generation, accurate CV-before-gate routing,
visible mute cursors and explicit old-project compatibility checks.

## Adoption boundary

A firmware integration was considered, then declined for this delivery. Fetching
the remaining design, memory-map, pin-map and priority documents, and additional
portable traversal source, failed through both the raw GitHub host and a pinned
CDN mirror (DNS failure). The web connector could read the changelog but not
those four documents. They were **not** read; full firmware review is incomplete.
No firmware, simulator, toolchain, flash utility or hardware code was installed,
run, copied into the build or linked. No claim of a complete PER|FORMER port.

Pattern / Motion instead implements the documented behaviour in first-party
Optisynth code. The condition evaluator is a direct truth table with independent
state, not the upstream helper/dependency graph. Traversal, probability, control
lanes, bounded sample-offset scheduling, scale quantization, desktop editing and
serialization are local implementations. There is no new third-party runtime
dependency in 0.0.8. Source descriptions and licences remain research evidence.

## Behaviour carried into implementation

The distinction between Previous **condition** and previous sounded gate matters:
Always and Previous leave that state alone; a failed chance test does not update
it. Pendulum repeats endpoints; ping-pong does not. Rotation is nondestructive;
shifting changes stored steps. Selection and playheads remain separate. A
snapshot captures musical state temporarily; rack Save persists it. Independent
lane cursors make polymetric results explainable.

## Side effects and qualification boundary

The first-party module writes only its note-event buffer, two control buffers
and atomic display state. The UI writes named rack parameters; generators and
clipboard edits publish coherent transactions. It opens no network sockets, MIDI
ports, audio device or files. The existing standalone opens the selected system
audio output only on a user's Start audio/Play click; rack Save/Open are existing
explicit file operations. Removal/disconnection is the ordinary rack graph path,
which retires the note source without preserving stuck notes.

No Ableton, hardware MIDI, firmware calibration or flashing is involved. Core
failure risks are mistimed/stuck notes, wrong conditions, incoherent edits and
silent CV assignments. Focused event, ownership, atomic-edit, recall and native
widget checks target those risks. The two original FX starters exercise both
control outputs through actual matrix destinations, not display-only lanes.
