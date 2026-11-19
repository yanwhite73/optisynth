# First playable sequencers — 19 November 2026

Request 43 resumes implementation after the source and 54-image interface study.
This increment implements our own Pattern, Euclidean and Stage algorithms; it
does not adopt or execute third-party sequencer code. The independent held-note
arpeggiator, chord/phrase composition and arranger remain subsequent increments.

## Mechanism and acceptance

One audio-owned 48 kHz transport drives all reachable sequencers. UI atomics set
tempo/run/restart; UI timers only display audio-published playheads. Stop, mute,
clock discontinuities and timing changes release owned notes. Play resumes;
Restart returns to beat zero. Device stop also stops transport. Tempo is saved;
run state and playheads are deliberately transient.

Event outputs connect to optional `notes` inputs on sources, envelopes, resonators
and retriggerable LFOs. Unpatched inputs retain the existing shared keyboard.
Connected inputs use their own bounded held-note ledger, latest-note priority and
sample-offset processing splits. Audio/control cables retain their existing path.
These are independent monophonic destinations, not general rack polyphony.
Plan changes retain the existing 5 ms retirement fade; no event state is carried
to a new route. A newly connected sequencer joins the current grid boundary.

- Pattern: 16 stored steps, editable length, pitch/velocity/rest per step.
- Euclidean: up to 32 steps, hits/rotation, pitch, velocity and gate; a ring shows
  the generated pattern rather than implying separately editable stored steps.
- Stage: eight stored stages, length, pitch, pulse count, Repeat/Hold/Rest.
  The timeline width reflects pulse duration; Hold sustains through its stage.
- Shared frame: module enable, division, length, gate, explicit note socket,
  selected-step inspector, distinct selected and playing indicators.

UI references: Bespoke's distinct note/Euclidean editors, PER|FORMER's contextual
inspector and OXI Saga's duration-aware stages. Use our own artwork/widgets.
Pattern and Stage use different authoritative parameters; switching module type
is replacement, not an implied lossless conversion. Settings and notes serialize
through stable module/parameter identities in the existing rack schema, with a
new rack version for transport tempo.

Qualify: exact event offsets across callback sizes; independent routes/fan-out;
tempo/start/stop/restart/mute; rests and stage dwell; complete saved recall;
no allocation during render; removal/rerouting; native editing and screenshots.
Render audible starter fixtures offline. Keep host/plugin verification separate.
