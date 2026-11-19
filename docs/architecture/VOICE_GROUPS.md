# Voice groups and analog pad modules

Requests 57–58, 19 November 2026. This is the contract for polyphony in the rack and
for the analog-style modules. The circuit evidence behind the modules is in
`../research/instruments/CLASSIC_PAD_ARCHITECTURE.md`.

## Voice group rule

- **Boundary.** A rack may contain one **Voice mixer** (`voice.mix`). Every
  module patched upstream of its `in` socket through audio or control cables
  runs once per voice: 8 copies, each with its own DSP state, all sharing one
  set of knobs. Everything after the mixer runs once.
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
1. the voice already playing the same key (retrigger);
2. the free voice released longest ago;
3. otherwise the oldest held voice (steal).

**Behaviour:**
- Changes are applied at the event's sample offset within the 24-frame block.
- Envelopes restart their attack from the current level, so stolen or
  retriggered voices do not click.
- Lowering the mixer's Voices control releases the surplus voices.
- When a new patch is swapped in, the Player re-sends every held key to it.

**Hardware MIDI** opens only when Start audio is pressed. All inputs are merged
to channel 1. Supported: velocity, mod wheel (CC1, as `Note::wheel` and the
Wheel / bend module) and pitch bend (±2 semitones, as `Note::bend`). The sustain
pedal holds note-offs until it is released.

## Modules

| Module | Behaviour |
| --- | --- |
| Analog VCO | CEM3340-style saw/pulse/triangle with PolyBLEP, exponential FM input, PWM, hard sync on rising zero crossings, per-voice tolerance offset plus slow random drift (deterministic per voice) |
| OB filter | 2-pole zero-delay state-variable with a soft-clipped (diode-limited) band-pass state and SEM LP→notch→HP crossfade or band-pass; or a 4-pole OTA cascade with input saturation. 2× oversampled (31-tap halfband, ~0.3 ms), per-sample cutoff from mod and FM inputs, key tracking, ~2 Hz output DC block |
| Analog envelope | CEM3310-style RC stages with exponential times. Attack charges toward 1.3 and turns at 1.0. Times up to 30 s attack/decay and 60 s release; velocity amount |
| OTA VCA | Control × audio with OTA input saturation |
| Voice mixer | Per-voice constant-power pan spread (OB-X/Xpander style), level, voice count |
| Wheel / bend | MIDI mod wheel and pitch bend as control outputs |
| Reverb, Chorus, Stereo delay, Phaser | Stereo wrappers of the inherited first-party effects; buffers are allocated at preparation |

## Measured

- **CPU:** an 8-voice pad (two VCOs, OB filter, two envelopes, VCA, then chorus
  and reverb) renders 10 s in 0.65 s on one core (6.5%), with no allocation in
  render.
- **Checks:** all modular checks pass normally and under ASan/UBSan.
- **Native app:** both pad starters load and display. A five-note chord held for
  8 s reproduces identical stereo audio after JSON recall. Peaks at the app's
  0.25 output gain are 0.29 (OB swell) and 0.26 (Prophet).

## Not done

- **Glide, unison and per-voice LFOs:** an LFO patched by cable into a voice is
  copied per voice; one routed through the matrix stays shared.
- **More polyphony:** voice counts above 8.
- **More chip models:** Xpander pole mixing, Prophet poly-mod into oscillator
  pitch and pulse width (filter FM exists), and the SSM2040 Rev 2 filter.
- **Tuning tables.**
- **Measured comparison with hardware or Arturia references.**
- **User listening:** the offline WAVs are references, not a listening verdict.
