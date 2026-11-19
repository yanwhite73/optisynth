# Voice groups and analog pad modules

Requests 57–58, 19 November 2026. This is the contract for polyphony in the rack and
for the analog-style modules. The circuit evidence behind the modules is in
`../research/instruments/CLASSIC_PAD_ARCHITECTURE.md`.

## Voice group rule

- **Boundary.** A rack may contain one **Voice mixer** (`voice.mix`). Every
  module patched upstream of its `in` socket through audio or control cables
  runs once per voice: up to 16 copies, each with its own DSP state, all
  sharing one set of knobs. Everything after the mixer runs once.
- **Racks without a Voice mixer** behave exactly as before, with mono note
  context. The 251 pre-existing modular checks are unchanged.
- **Always global, even when upstream:** sequencers, Spectral, `mod.wheel`, the
  `fx.*` effects and the mixer itself. Their outputs reach voices as shared
  signals. Spectral inside a voice group is rejected because it is already
  polyphonic.
- **Matrix routes:** a global source can modulate a voice parameter, with the
  same depth for every voice. A per-voice source cannot modulate anything after
  the mixer.
- **Rejected with a message:**
  - a second Voice mixer;
  - a voice module patched straight to the output or to a global module;
  - a sequencer patched into a module inside the voice.

  Sequencers play a voice group through the mixer's `notes` input.

## Notes and allocation

**Note sources.** The voice allocator reads the mixer's `notes` input if it is
patched; otherwise it reads keyboard events.
- Keyboard events come from the on-screen keyboard, typing and hardware MIDI.
- The UI pushes them to `Player::noteEvent`, a lock-free SPSC ring drained at the
  start of each audio block.
- The existing mono note path is unchanged.

**Voice choice.** On a new note the allocator picks, in order:
1. the voices already playing the same key (retrigger);
2. the free voices released longest ago;
3. if Steal is on, the quietest held voices (by level estimate, ties to the
   oldest). New voices are protected until their level is measured. With Steal
   off, a note with no free voice is not played.

**Voice mixer controls (0.0.15):**
- **Voices:** 1–16.
- **Unison:** 1–8 voices per key, spread symmetrically by Detune (cents). A key's
  voices retrigger and release together.
- **Glide:** time in seconds to cover about 95% of the interval. Each voice
  glides from its own previous pitch, as on the Prophet-5.
- **Tuning:** Equal; Just 5-limit; Just 7-limit; Pythagorean; or Harmonic series
  (non-octave: the root key in C4's octave is partial 8, each key above is the
  next partial). Plus a **Root** note. Tuning, glide and detune reach modules
  as `Note::bend`, so the analog VCO and the filter's key tracking follow them.
  Older modules ignore bend.

**Behaviour:**
- Changes are applied at the event's sample offset within the 24-frame block.
- Envelopes restart their attack from the current level, so stolen or
  retriggered voices do not click.
- Lowering the Voices control releases the surplus voices.
- When a new patch is swapped in, the Player re-sends every held key to it.
- **Sleeping:** a released voice is no longer processed once its output has
  stayed below −100 dBFS for 0.5 s. Waking snaps smoothed controls to their
  targets.

**Hardware MIDI** opens only when Start audio is pressed. All inputs are merged
to channel 1. Supported: velocity, mod wheel (CC1, as `Note::wheel` and the
Wheel / bend module) and pitch bend (±2 semitones, as `Note::bend`). The sustain
pedal holds note-offs until it is released.

## Modules

| Module | Behaviour |
| --- | --- |
| Analog VCO | CEM3340-style saw/pulse (zero-mean)/triangle with PolyBLEP, exponential FM input, PWM, hard sync on rising zero crossings, per-voice tolerance offset plus slow random drift (deterministic per voice) |
| OB filter | Quality 1×/2×/4× oversampling. Xpander mode: 15 pole-mixing responses (LP 1–4, HP 1–3, BP 2/4, notch, phase, and four "+ 1-pole LP" variants) from weighted sums of the 4-pole cascade's input and stage outputs. Classic: 2-pole zero-delay state-variable with a soft-clipped (diode-limited) band-pass state and SEM LP→notch→HP crossfade or band-pass; or a 4-pole OTA cascade with input saturation. 2× oversampled (31-tap halfband, ~0.3 ms), per-sample cutoff from mod and FM inputs, key tracking, ~2 Hz output DC block |
| Analog envelope | CEM3310-style RC stages with exponential times. Attack charges toward 1.3 and turns at 1.0. Times up to 30 s attack/decay and 60 s release; velocity amount |
| OTA VCA | Control × audio with OTA input saturation |
| Poly-mod | Prophet-style per-voice mix of oscillator B and filter envelope. Feeds oscillator A FM and PWM and filter FM; the destination depth knobs act as the routing switches |
| Voice mixer | Per-voice constant-power pan spread (OB-X/Xpander style), level, voice count |
| Wheel / bend | MIDI mod wheel and pitch bend as control outputs |
| Reverb, Chorus, Stereo delay, Phaser | Stereo wrappers of the inherited first-party effects; buffers are allocated at preparation |

## Measured

- **CPU:** the OB pad (8 voices) costs 7% of one core. The Xpander pad (16
  voices: 8 keys × unison 2, 2× oversampled, with chorus and reverb) costs 16%.
  Neither allocates in render.
- **Checks:** 324 modular checks pass normally and under ASan/UBSan. Steal,
  unison, glide, the exact JI and harmonic ratios, sleep/wake, Xpander modes
  and quality-level agreement have dedicated checks.

## Not done

- **CPU-budget polyphony, staggered starts, multicore voices and SIMD across
  voices:** see `../research/platforms/PERFORMANCE_QUALITY_LESSONS.md`.
- **Legato and mono priority.**
- **The Prophet-5 Rev 2 SSM2040 filter.**
- **Scala file import:** the built-in tables are not yet loaded from files.
- **Measured comparison with hardware or Arturia references:** plugin hosting
  needs Yan's approval.
- **User listening.**
