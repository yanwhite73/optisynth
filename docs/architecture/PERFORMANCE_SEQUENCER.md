# Modular performance sequencer — implementation contract

Requests 49 and 54: a complete four-track instrument, informed by PER|FORMER's
pattern/perform/song pages and Impromptu Foundry's separate track outputs. The
stored UI atlas and manuals are the references; no upstream firmware or artwork
is adopted. Existing Pattern, Euclidean and Stage remain separate modules.

- Four independent tracks, eight patterns per track, 64 steps per pattern.
- Each track exports its own note events, gate, note-relative pitch bend and CV.
  Oscillator pitch-bend inputs support tied pitch changes and slides. Bend uses
  48 semitones per control unit; it supplements the note event's base pitch.
- Patterns retain length, clock division, traversal, rotation, scale/root and
  swing. Steps retain pitch, velocity, gate, probability, ratchets, conditions,
  outgoing tie, slide time, CV and protection from generation.
- Selection never launches. Pattern cells have separate Edit and Launch actions.
  Launches may be immediate or queued to a beat/bar/two-bar boundary; cancel is
  explicit. A launch releases the old track's note and restarts its local phase.
- Sixteen chain rows select a pattern for each track and a duration in bars.
  This bounded chain owns only this module's tracks, not arbitrary rack modules;
  the separately researched general conductor remains a different future module.
- Track mute/solo preserve traversal phase; track reset explicitly restarts it.
  Transport stop/seek/reset and pattern switches release owned notes. Fill and
  transpose affect actual playback. UI timers only display state.
- The editor has a four-track pattern launcher, detailed step/pitch/property
  editing and a chain table. Edit, play and queued positions remain distinct.
  Copy/paste, undo/redo and protected variation have defined edit scopes.
- Rack saves include all patterns and chains, including dormant modules. Play
  cursors and pending launches are transient. Old rack versions remain readable.

Realtime implementation: prepare immutable banks off-thread, publish through a
bounded mailbox and retire old banks off-thread. Do not expand every module's
scalar parameter array to hold all sequencer banks. Per-output event buses are
resolved at graph preparation. Processing is bounded and allocates nothing.

Acceptance: four independently routed audible parts; edit inactive B while A
plays; queue/cancel B without changing edit selection; execute a repeating chain;
tie without retrigger and bend through a slide; stop with no held notes; exact
save/reload playback; identical events across callback sizes; zero callback
allocations. Native UI interaction and visual review are separate qualification
steps and must not be claimed from compilation or offline DSP checks.

## Research translated into operations

| Reference | Applied behaviour | Implementation boundary |
| --- | --- | --- |
| [PER\|FORMER P06–P09](../ui/sequencers/performer.md), [manual](https://westlicht.github.io/performer/manual/) | Separate edit/play/queued patterns, boundary launch/cancel, mute/fill, phrase arrangement | Four tracks and eight banks here; no firmware, hardware drivers or upstream artwork copied |
| [Impromptu / Foundry](../ui/sequencers/impromptu.md), [manual](https://marcboule.github.io/ImpromptuModular/) | Four independent destinations; ties, slide and pattern/chain organisation | Dedicated note, gate, bend and CV sockets for each track; destination voices remain monophonic |
| [Bespoke B02/B04](../ui/sequencers/bespoke.md), [workflow study](../ui/sequencers/WORKFLOWS.md) | Protected steps and visible editing scope | Variation is previewed before Apply; undo changes only affected fields |

The implementation follows the reviewed behaviours independently. It is not a
port or a complete clone of any reference. OXI chord/stochastic/harmonic modes,
Marbles-style memory, Kria's independent parameter loops, and a held-note arp
remain distinct work; a four-track pattern bank does not implement those models.

## Playing and editing

Start with **Performance / Four currents** (also the new startup rack) or
**Performance / Clockwork garden**. Opening the editor does not start audio. **PLAY**
starts transport through the app; **STOP** releases the sequencer's notes.

1. Each track has A–H banks. Click a cell to edit; click its triangle to launch.
   **Launch column** queues the selected letter on all four tracks. **Cancel
   queued** cancels all pending launches. The boundary menu is explicit.
2. Each pattern retains its first/last step, division, traversal, rotation,
   scale/root and swing. The four page buttons expose all 64 steps. Click a step,
   Shift-select a range, enter pitches using the displayed keys, or drag the
   selected property lane. Double/right-click toggles a rest.
3. **Tie to next** belongs to the outgoing step. Set **Slide / seconds** on the
   arriving step. Route that track's bend socket to a compatible oscillator to
   hear pitch motion without retriggering the envelope. The bass starter does
   this; the whole Spectral module does not yet accept this bend socket.
4. **Preview variation** leaves saved/playing data untouched. Protected steps are
   preserved when applying it. Copy/paste and clone act on the named pattern;
   undo/redo merge changed fields so unrelated bank edits survive.
5. **CHAIN** opens sixteen rows. Each chooses four pattern letters and a duration
   in bars. Set the first/last row and enable **PLAY CHAIN**. The highlighted row
   and remaining beats come from the audio engine. Opening the table alone does
   not arm the chain. Mute/solo continue track phase; Reset restarts one track.

Save the rack from the main window. Rack v5 embeds every bank and chain, including
inactive modules. Versions 1–4 remain readable. Editing selection and pending
launches are transient; stop/start retains the launched banks during the session,
while loading a rack starts each track at its saved initial bank.

## Runtime and qualification limits

Note edges and launches are sample-timed in the 48 kHz engine. Bend/CV buffers are
sample-valued; oscillator frequency reads follow the existing 24-sample processing
quantum. This is not an audio-rate parameter-modulation claim. Gate is 0/1, CV is
−1..1 and bend is relative to the held note, at 48 semitones per unit. Pitch-bend
inputs currently exist on the VA and sine oscillators.

The complete bank is immutable on the audio thread. Message-thread edits publish
through a bounded mailbox, and message-thread collection disposes retired banks.
Four preallocated event buses prevent one track's notes leaking into another
output. Normal and ASan/UBSan checks include callback-size independence, note
ownership, exact queued launches, chain boundaries and allocation-free rendering.
Native interaction tests and component snapshots exercise the real JUCE editor
without opening an audio device; desktop window launch remains a separate check.
