# Probability and Orbit — request 59

Two independent rack modules, with original source and drawings. The supplied
Stochas T01 and Bespoke B03 images are interaction/model references; no upstream
code or artwork is adopted. OXI OX01/OX06 reinforce labelled grid axes and the
difference between pitch choice and gate chance.

**Probability:** 16 pitch rows × 32 time columns, viewed in 16-step pages. Relative
cell weights select one note per clock; gate probability is a separate step value.
Scale/root/octave label the rows. Step velocity, gate, ratchets, conditions and
generation protection are editable. Repeat choices fixes the per-column pitch/gate random draws;
fresh mode advances them. Conditional passes still advance, and random traversal
is not frozen by Repeat choices. New choices changes the saved seed. Painting, erasing, undo, protected generation preview,
snapshot/restore and capturing the realised notes act on saved module parameters.
An empty column is a rest. Notes/gate/pitch-CV outputs are independently routable.

**Orbit:** four independently clocked Euclidean rings, with length, hits,
rotation, rate, pitch, chance, ratchets, swing, accent and per-cycle rotation.
Each ring has separate note/gate/accent outputs; a combined note bus is available.
Mute/solo continue phase. Dragging a ring rotates its pattern. Actual playheads
and effective rotations come from the audio engine.

Existing scalar storage is sufficient (Probability ≤768 slots); no global array
increase or new rack format is needed. Only active modules are prepared. Shared
edit transactions prevent half-painted grids reaching the renderer. Timing and
randomness live in DSP, with bounded work, instance-local random state and no
callback allocation. GUI timers only display state. Scroll gestures navigate.

Acceptance: weighted choices respect the painted cells; repeat/fresh semantics
are distinct and deterministic; empty/muted steps release notes; ratchets/launch
edges are sample timed and callback-size independent. Four Orbit outputs must
remain separate. Verify full save/reload audio, deliberate native grid/ring
gestures, undo, scroll safety, and actual normal/minimum-size captures. Preserve
the concurrent pad-voice implementation and the existing stable app location.

## Using the devices

Probability's column weights are relative: 75 and 25 give two candidate pitches
in a 3:1 ratio, not two independent 75%/25% gates. Empty columns are rests. Click
row labels to mute a pitch. Paint/Erase drag across cells; Select only changes the
inspector. The lower lane draws velocity, gate, chance or ratchets. Horizontal
scroll over the grid changes pages; vertical scroll and slider wheels never edit
notes. The displayed octave uses MIDI 60 = C4; the saved octave is MIDI/12.

First/last steps bound the loop. Both remain saved independently; if first exceeds
last, the engine plays only the last step. Step conditions count passes through
the loop. Capture uses the most recently realised choice in each visited column;
it preserves unvisited columns and existing articulation, density and conditions.
Snapshot, Restore, Undo and Redo are temporary editing tools; save the rack for
persistent recall. HOLD FILL is momentary. RESET releases and restarts this module
at the current song time without resetting other modules.

Orbit's CLOCK values are beat divisions. STEPS/HITS/ROTATION describe each ring;
click a ring row to select its articulation strip. Accent every counts step
positions, with zero disabling accents. ROTATE PER CYCLE adds a signed offset on
each full revolution. Mute/solo suppress notes while clocks keep moving. Shared
transport starts/stops all active sequencers. All notes combines the four source
identities; individual ring outputs keep routing explicit.

## Boundaries

These are independent implementations of weighted selection and Euclidean
rhythms, not ports of Stochas or Bespoke. Stochas dependency chains and layered
polyphonic selection, OXI's complete modes, Kria's independent parameter-loop
model, Marbles-style memory, and a held-note arpeggiator remain separate work.
Both new modules share the existing clock/patch infrastructure but keep their own
editors, parameter sets and output sockets. No new external code is built.
