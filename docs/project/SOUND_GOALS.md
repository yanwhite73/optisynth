# Sound and sequencing goals

Recorded 19 November 2026 from request 44 (verbatim in `USER_REQUESTS.md`). These
are design targets that steer priorities. They are not implemented features.

## Next playable patches for 0.0.18

Request 80, 22 November 2026. The November 19 inventory below predates the
polyphonic analog voice and rack effects. These are proposed listening tests.

1. **Organic squelch.** Sequence a saw through the OB filter in four-pole
   low-pass mode. Start around 300 Hz, with high resonance, moderate drive and
   a positive two-to-three-octave filter envelope decaying over 100–250 ms.
   Give the amp a longer decay so the sweep can be heard. Use step velocity on
   the filter envelope and tie an occasional pair. Listen for an accented note
   opening into a vowel while neighbouring notes stay short and woody. A later
   strikeable low-pass gate could couple brightness and level as each note
   dies away. [Optomix strike and damping](https://www.makenoisemusic.com/modules/optomix/)
2. **Tinkly delay lines.** Start with sparse, high Plaits modal percussion hits.
   Set the material and decay until a single dry hit rings like glass. The
   existing stereo delay accepts independent millisecond times and has filtered
   feedback. At 120 BPM try 250 ms and 375 ms, then leave a bar empty for the
   echoes. First engineering priority is to expose beat divisions per side and
   cross-feedback in the rack and feed the patch tempo to the delay. Listen for
   the same interlocking rhythm after changing tempo during a tail. The 4ms
   two-channel clock arrangement is a useful interface reference.
   [4ms Dual Looping Delay manual](https://4mscompany.com/DLD/manual/DLD-manual-1.1-v5.pdf)
3. **String-like swells.** Start from the factory OB swell. Try two saws a few
   cents apart, a two-second amp attack and a four-second filter attack. Bring
   the stereo chorus up with reverb down. Listen for individual chord tones
   blooming and moving against each other, including as one chord releases
   into the next. Compare the existing chorus with a dedicated string-machine
   ensemble only if the held chord still lacks movement. Bow friction under
   continuous pressure would be a separate physical-modelling target.
   [Solina ensemble reference](https://www.arturia.com/store/solina-v)

Suggested first delivery is three named patches with dry and processed example
phrases, plus the rack delay controls. Judge the sounds at matched loudness.
Implementation evidence is in `src/modular/analog.cpp`, `src/modular/modules.cpp`,
`src/core/effects.cpp` and the factory entries in `app/PatchIO.h`.

## The goals

1. Modular sequencing in the style of Robert Rich and Steve Roach: ambient,
   psychedelic Berlin School.
2. Organic, detailed sounds that evolve slowly.
3. Lush analog swells and pads, of the kind Oberheim and Prophet polysynths make.

## What the artists' own accounts establish

Checked 19 November 2026. "Primary" means the artist's own words, quoted on the
page. No recording has been analysed yet.

| Claim | Source | Strength |
| --- | --- | --- |
| Roach calls the Oberheim Xpander "my life time instrument", used in nearly all concerts since 1984, and calls "the matrix patching" "a vital part of the architecture" | [Synth History, three questions](https://www.synthhistory.com/post/three-questions-with-steve-roach) | Primary |
| Roach keeps an OB-8 in the studio and uses an OBX-8 live for "the classic OB sound". His live rig includes an analog sequencer, a hardware effects rack and a 32-channel board he treats as a "hands-on instrument" | Same | Primary |
| The *Structures from Silence* title track was played on the OB-8 | [Gearspace thread](https://gearspace.com/threads/welcome-steve-roach.1108786/), [Self-Titled review](https://selftitledmag.substack.com/p/steve-roach-structures-from-sound-review) | Secondary |
| A 1987 magazine lists an Xpander, OB-8 and full Oberheim system in Roach's gear; a forum post adds the Matrix-12 | [Muzines archive](https://www.muzines.co.uk/articles/the-sound-art-of-programming/2087), Gearspace | Secondary; Matrix-12 unverified |
| Rich's Prophet-5 "became my primary keyboard synth for decades" | [Sequential artist page](https://sequential.com/artists/robert-rich/) | Primary |
| Rich: just intonation is "a basic part of my music". He uses non-octave tables built on the harmonic series, where "the tuning itself becomes a part of the timbral component of the sound" | Same; [15 Questions on alternate tuning](https://15questions.net/interview/robert-rich-alternative-tuning-systems/) | Primary |
| Rich wants "roundness or woody-ness or earthiness", sounds "coming from a place", and praises the Prophet-6's "smooth, thick resonance" | Sequential artist page | Primary |
| Rich's earlier live rig used long looping delays, echoes and reverbs | Search summary of [robertrich.com interviews](https://robertrich.com/media/interviews/keyboards-magazine-germany-2003/); page not opened | Unverified |

Not established: that Roach used a Prophet-5, or the settings on any particular
recording.

**What this means for the project.** The Xpander has been a research target since
request 1. It turns out to be the most relevant reference instrument, and its
matrix modulation is the model for Optisynth's own matrix. The Prophet-5 (Rich)
and the OB-8 (Roach's pads) come next. Rich's reliance on just intonation makes
a tuning system a core requirement rather than an extra.

## What the engine needs (proposed, not implemented)

### Lush polyphonic swells (Oberheim / Prophet)

| Need | Current state |
| --- | --- |
| Polyphonic voices: 6–8 or more, with release tails overlapping | Rack is monophonic. Only the whole Spectral engine is polyphonic (32 voices, unison up to 6) |
| Two oscillators per voice, detune in cents, per-voice drift ("slop") | Plaits VA oscillator; detune only through the semitone Pitch knob; no drift |
| Filters: SEM-style 12 dB state-variable with LP–notch–HP morph (Oberheim); 4-pole low-pass with drive (Prophet); Xpander's 15-mode filter as a research target | Digital SVF LP/BP/HP (clean, 12 dB); Spectral filter effect |
| Envelopes long enough for swells: 20–30 s attack and release | Maximum attack 8 s, decay and release 12 s |
| Prophet-style poly-mod: oscillator B and filter envelope modulate oscillator A pitch, pulse width and the filter at audio rate | The matrix is control-rate, one value per 24 samples |
| Per-voice stereo spread, then stereo chorus or ensemble after the voices are summed | No voices to spread; chorus exists in the core library but not in the rack |
| Played expression: velocity, mod wheel, aftertouch, pitch bend | No hardware MIDI input; velocity fixed at 0.8 |
| Headroom for 8 summed voices: soft saturation or limiting | Hard clamp at ±0.95 |

### Organic, detailed, evolving

- **Slow random modulation:** smooth random and drift, sample-and-hold, slew and a
  bounded random walk, plus matrix depth as a modulation target. Today there is
  only a 4-shape LFO (minimum rate 0.01 Hz is adequate).
- **Per-voice variation:** each voice differs slightly in pitch drift and filter
  offset, as analog voices do.
- **Noise and physical texture:** a noise source (white, pink, coloured) to excite
  the existing modal resonator and filters. There is no noise module yet.
- **Time effects as modules:** long reverb, modulated and looping delays.
  `src/core/effects.cpp` already holds 13 of our own effects, including a reverb
  up to 50 s, stereo, modulated and ping-pong delays, chorus and phaser. Only the
  filter is exposed in the rack. Long-tail upgrades already researched:
  Airwindows Galactic (MIT) and the Clouds reverb (MIT). Plateau is GPL-3, which
  affects VST distribution.
- **Tuning:** Scala `.scl`/`.kbm` tables, just intonation and non-octave scales,
  applied to every pitched module and to sequencer pitches.
- **Granular texture:** already queued.

### Ambient, psychedelic Berlin School sequencing

The sibling Optiseq project already holds a sourced technique table:
`../../../ableton/sequencer/docs/research.md`, section 3. Reuse it rather than
redo it. These parts bear directly on the modules:

- Transpose a running sequence from the keyboard, moving the whole cell (Schulze, Moog).
- Per-step control rows besides pitch, such as filter and VCA (Moog 960), sent
  to the matrix.
- Cells of different lengths running together, so they phase. Separate
  sequencers with different lengths partly do this already.
- Add and drop steps while playing; ratchets and repeats (Stage pulses exist).
- Tempo-synced delay used as the rhythm engine.
- Process over minutes: filter, resonance, decay and delay feedback move slowly,
  and sequences fade in over minutes.
- Glide for sequenced lines; optional pitch drift for imperfection.
- Sequencer pitches pass through the tuning table, so sequences can be just-intoned.

## Acceptance targets (proposed)

Offline render checks come first. Yan's listening is a separate, later step.

- **Reference pad:** 8 voices, two oscillators, 24 dB low-pass, 10 s attack,
  15 s release, stereo chorus and long reverb. Hold and release a 4-note chord:
  no clipping before the safety guard, measurable per-voice drift within a
  stated range in cents, and no stepping in a resonant filter sweep (checked on
  a spectrogram).
- **Sequence:** a 16-step cell transposed from the keyboard while running, lanes
  of 13 and 16 steps phasing, a filter lane and a synced delay. The render is
  identical for a given seed.
- **Just intonation:** a 7-limit table produces exact ratios (within 0.01 cents)
  in every pitched module.
- **CPU:** benchmark the 8-voice pad at 48 kHz with 128- and 256-sample device
  blocks and record the real-time ratio.

## Open questions for Yan

- Which recordings define the target? Candidates: Roach, *Structures from Silence*
  and *Empetus*; Rich and Roach, *Strata* and *Soma*; Rich, *Rainforest* and
  *Trances/Drones*. Ask; do not assume the most-cited track.
- How many pad voices are needed: 6, 8 or 16?
- Should pads come from polyphonic voice groups in the rack (recommended; this is
  what `../architecture/ARCHITECTURE.md` intends) or from a dedicated poly engine
  like Spectral?

## Factory sounds by musical role (requests 99–102)

Assign each factory sound one role and audition it unaccompanied in that role.

| Role | Arrangement job | Listening check |
| --- | --- | --- |
| Pad | Carry harmony through sustained and overlapping notes. Give each chord time to change colour. | Hold a common tone across a chord change. The upper voice can bloom without generating a rival rhythmic line. |
| Lead | Carry the foreground melody. Give the player control over attack, note joins and release; make articulation and pitch legible against accompaniment. | Play a phrase with a held peak, two connected notes, and a deliberate rest. Check that expressive changes read clearly. |
| Berlin-school ostinato | Maintain a recognisable repeated cell. Variation can come from unequal accent, tone or cycle lengths. Any rhythmic echo is a deliberately placed answering event. | Repeat the same cell with a gap. Count the dry attacks and their answers separately. Mute the echo; the dry cell should still have identity and motion. |

Use separate preview performances. A shared eight-note qualification phrase remains useful for comparing versions. Peak, finiteness and snapshot checks say nothing about musical usefulness. Treat player acceptance as a separate outcome.
