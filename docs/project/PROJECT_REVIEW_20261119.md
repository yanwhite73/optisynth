# Project review — 19 November 2026

Request 44 asked for a review of the project and added new goals; the goals are
recorded in [SOUND_GOALS.md](SOUND_GOALS.md). This review measures the current
project against those goals as well as against its own architecture.

## Scope and method

- **Read:** the modular runtime (`graph`, `runtime`, `player`, `modules`,
  `timing`, `rack`), the audio and MIDI wiring in the app, the effect library
  index, the test harness, recorded qualification logs, `STATUS`,
  `USER_REQUESTS`, `ARCHITECTURE`, `RESEARCH` and `FIRST_SEQUENCERS`.
- **Sequencers:** request 43's work, read while a concurrent Codex session
  (`codex:01a1bff2…`) was still qualifying it. It was then committed as
  `73ac3cc` (standalone 0.0.7: 198 modular checks, normal and ASan/UBSan, per
  its receipt).
- **Not run:** no build or tests. The Codex session was using the same `build/`
  directory and all three heavy-workload slots were taken. Test results cited
  below come from recorded logs and receipts.
- **Not done:** no listening, no CPU measurement, no Live or VST3 checks.

## Summary

The engineering foundations are solid. The main risk is order of work: the
features the new goals depend on most keep being deferred behind research and
UI work. Those features are polyphony, effects in the rack, slow random
modulation, tuning, MIDI input and long envelopes. None of them exists in the
rack yet.

## What is working well

- Real-time discipline. Graphs are planned and prepared off the audio thread,
  and only reachable modules are built. Scalar edits use resolved atomics.
  Plans swap through a single-producer/single-consumer mailbox with a 5 ms
  crossfade, and retired plans are deleted on the UI thread. The tests count
  allocations in the render path.
- One schema (`panels()`) drives the factories, the native panels and the saved
  rack document, so the editor, file format and future agent API share stable
  module and parameter IDs.
- Typed ports with mono and stereo audio, cycle rejection, dormant-node
  validation and versioned rack files that still import older versions.
- Sample-accurate events. `NoteEventBlock` and `HeldNotes` carry sample offsets
  and per-source note identities, and the runtime splits processing at event
  offsets. This is the right base for MIDI, sequencers and polyphony.
- A good inherited asset that is not yet used: 13 of our own effects in
  `src/core/effects.cpp` with allocation-free `process()`, beat-synced delay
  times and reverb tails up to 50 s.
- Provenance and licence discipline: pinned Mutable sources with hashes,
  explicit adoption boundaries, ASan/UBSan runs and verified backups.

## Findings, ordered by impact on the goals

### 1. The rack is monophonic (high)

**Evidence:**
- `rack.h` `validateRack` rejects any scope other than a mono voice.
- `Runtime` renders one shared `Note`.
- `Player::note` is a single packed atomic, so the latest write wins.

Lush Oberheim and Prophet swells need held chords with overlapping releases.
Today the only polyphonic path is the whole Spectral engine (32 voices,
unison up to 6), which is not a VA voice.

**Recommendation:** make voice groups the next engine milestone after request 43.
`ARCHITECTURE.md` already specifies the design: copy the voice subgraph per
note and run a global subgraph after an explicit mix boundary. It just has never
been scheduled.

### 2. Time effects exist but are not in the rack (high value, low cost)

**Evidence:** `makeEffect` provides reverb, three delays, chorus, phaser and
flanger, but the rack wraps only `filter` (`modules.cpp`, `Filter` class).

Ambient music in this style depends on reverb and delay; Rich's earlier rig
used long looping delays. Wrapping the existing effects as stereo modules gives
an immediate, audible payoff.

**Recommendation:**
- Wrap the existing reverb, delays, chorus and phaser as stereo rack modules.
- Freeverb-style comb reverbs can sound metallic on very long tails. Airwindows
  Galactic (MIT) and the Clouds reverb (MIT) are the researched upgrades.
  Plateau is GPL-3, which matters if the VST is distributed.
- Once voice groups exist, these effects must move to the global scope.

### 3. Modulation runs at control rate and samples sparsely (medium-high)

**Evidence:**
- Matrix routes read `m.source[0]`, one value per 24-sample block (2 kHz), in
  `runtime.cpp`.
- Both filter types read cutoff modulation from `b.inputs[1][0]` and set
  coefficients once per block.

This is adequate for slow ambient sweeps. It causes stepping on fast, resonant
envelope sweeps, and it rules out Prophet-style poly-mod, which needs
oscillator B to modulate oscillator A at audio rate. It may contribute to the
deferred aliasing report, where both envelope routes modulate the cutoff. That
cause is unconfirmed; the planned before/after render comparison still applies.

**Recommendation:**
- Interpolate filter cutoff across each block (or update it per sample).
- Add an audio-rate FM input to the oscillators.
- Keep the matrix at control rate for slow modulation.

### 4. Too few modulators for organic, evolving sound (medium-high)

**Evidence:**
- One LFO with sine, triangle, ramp and pulse.
- No smooth random, sample-and-hold, drift, slew or noise source.
- Envelopes stop at 8 s attack and 12 s decay and release.

**Recommendation:**
- Add a Drift/Random module (smooth random, sample-and-hold, bounded random
  walk), a Slew module and a Noise module.
- Extend envelope ranges to at least 30 s, or add a time-scale switch.
- Allow matrix depth to be a modulation destination.

### 5. No tuning system (medium; high for the Rich goal)

**Evidence:** `noteFrequency` uses an integer MIDI note plus a transpose in
semitones; sequencer pitches are integer MIDI notes.

Rich calls just intonation "a basic part of my music".

**Recommendation:** add one shared tuning table (Scala `.scl`/`.kbm`) used by
every pitched module, the sequencers and Spectral's note mapping, plus fine
tuning in cents on each oscillator.

### 6. No hardware MIDI input (medium)

**Evidence:**
- The app opens audio output only.
- Notes come from the on-screen and typing keyboard.
- Velocity is fixed at 0.8 in `Player`.

Pads cannot be played with expression. Mod wheel, aftertouch and pitch bend
also need to become control sources.

**Recommendation:** replace the packed note atomic with an SPSC queue of
sample-offset events; the `NoteEventBlock` design already fits.

### 7. Output protection is a hard clamp (low-medium)

**Evidence:** `Player` and `OutputConverter` both clamp to ±0.95.

Dense pads at high gain will clip harshly instead of compressing.

**Recommendation:**
- Keep the guard against non-finite values.
- Add a gentle soft-clip or limiter.
- Scale gain by voice count once polyphony exists.

### 8. Request 43 sequencers (observations)

- The Pattern, Euclidean and Stage design is coherent and event-timed. Separate
  lengths already allow phasing.
- For Berlin School the following are missing, all listed with sources in
  Optiseq's research §3:
  - keyboard transposition of a running sequence;
  - per-step control rows (filter, accent);
  - glide;
  - skip and probability;
  - seeded mutation.
- Step pitches are integer MIDI notes, so they will need the tuning-table mapping.
- A source can follow a sequencer while its envelope follows the keyboard. The
  starters connect both to the sequencer, but the editor should make the
  mismatch visible.

### 9. Readability (low-medium)

- Many files pack several statements onto one line: `runtime.cpp`,
  `modules.cpp`, `sequencer.cpp`, `player.h` and `RackEditor.h`. This makes
  diffs and reviews harder.
- The native self-check harness lives inside the production `RackSynth` class.

**Recommendation:** run a formatting pass, but only when no other session has
open edits. Running it now would conflict with the Codex session.

### 10. Documentation and process (medium)

- There are 68 Markdown documents against about 14,000 lines of code.
- `STATUS.md` (351 lines after 0.0.7) now leads with the current delivery, but
  its older sections still state superseded positions. For example, it says
  "device implementation remains paused", although 0.0.7 ships sequencers. Its
  "Limits/next" list puts the arpeggiator ahead of voice groups and does not
  list effects modules, which the new goals need most.

**Recommendation:**
- Cut `STATUS.md` to one screen: current state, next actions and known defects.
  Move the history into `ENGINEERING_LOG.md`.
- Weight further research by what blocks the goals: the Xpander's filter and
  matrix, and the Prophet-5's filter and poly-mod.
- Reuse Optiseq's Berlin School research instead of repeating it.

### 11. Plugin identity (low for now)

`plugin/` is still Optispectral's. A distinct Optisynth VST3 identity is
outstanding. Licence choices such as Plateau (GPL-3) will constrain it.

## Recommended order (proposal; Yan to confirm)

After request 43's sequencer increment is delivered:

1. **Effects and modulators.** Stereo rack modules for the existing reverb,
   delays, chorus and phaser; Noise, Drift/Random/S&H and Slew modules; longer
   envelope ranges. Small work with a large audible payoff.
2. **MIDI input.** Hardware MIDI through a sample-offset event queue, with
   velocity, mod wheel, aftertouch and pitch bend as control sources.
3. **Polyphony.** Voice groups with global effects scope, per-voice drift and
   pan spread, and a soft limiter.
4. **Analog pad voice.**
   - Detune in cents.
   - A 4-pole low-pass (Prophet-like) and a morphing SEM-style state-variable
     filter (Oberheim-like).
   - Smooth cutoff modulation and an oscillator FM input for poly-mod.
   - OB-style and Prophet-style starter pads.
5. **Tuning.** Tables with just-intonation starters.
6. **Berlin School sequencing.**
   - Keyboard transposition of a running sequence.
   - Control rows, glide, and seeded skip and probability.
   - Process modulators that work over minutes.

Steps 1 and 2 are independent of each other. Each step needs offline render
checks before Yan listens; those are separate stages and should be reported
separately.
