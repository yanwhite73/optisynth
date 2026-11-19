# Arturia reference comparison

Request 66, 19 November 2026. Yan approved loading his installed Arturia
instruments in a test tool, to compare them with Optisynth's analog modules.

## Pre-use record (written before the first load)

| Item | Finding | Evidence |
| --- | --- | --- |
| What they are | Audio Unit instruments (`aumu`, manufacturer `Artu`): SEM V2 `OBS2` 2.13.2, Matrix-12 V2 `Mat2` 2.13.4, Prophet V3 `PRO3` 3.9.1 | Bundle `Info.plist` AudioComponents |
| Plug-in behaviour | "Its interface and settings work the same way as in standalone mode." MIDI is supplied by the host. MIDI-controller support is import/export of mapping files | [Prophet V manual](https://r2.gear4music.com/media/94/944540/download_944540.pdf) §2.2.3, §3.2.11 (older edition; the current V3 manual was not obtained) |
| Licensing | Activated through Yan's Arturia account and Software Center. Unactivated behaviour is not documented in the manual read | Manual §2.1; [SEM V resources](https://www.arturia.com/products/software-instruments/sem-v/resources) |
| Writes | Presets and settings under `/Library/Arturia` and the user Library. Logging is off by default (`OutputFile` commented out in `resources/log.conf`). The plug-in may update its own preferences | Installed files |
| Network | Not documented. Bundles include Boost, JUCE and Poco, and Poco has networking libraries. Mitigation: record the test process's sockets with `lsof -i` during each run | `Documentation/license.txt` |
| MIDI ports | CoreMIDI is linked (standalone mode and controller integration). The test tool sends notes directly through the AU API and opens no MIDI endpoints | `otool -L` |
| Audio | Offline manual rendering in `AVAudioEngine`; no audio device is opened | Tool design |
| Removal | No installation step. The tool only instantiates already-installed components; deleting the tool removes it | — |
| Risk if wrong | A licence prompt could block (the run has a hard time limit), the plugin could reach the network (observed with `lsof`), or plugin preferences could change (only Arturia's own files) | — |

Documentation gap: Arturia's own manual server
(`downloads.arturia.com`) presented an expired TLS certificate. Certificate
checks were not bypassed, so the current SEM V and Matrix-12 V manuals were not
read.

Useful detail from the manual: Prophet V's DETUNE "gives a bit of instability
to tuning of the oscillators" when unison is off. That is the same per-voice
slop idea as Optisynth's VCO drift.

## Plan

Offline renders at 48 kHz. Neither side is assumed to be right.
1. Parameter dump: names, ranges and defaults.
2. CPU cost of an 8-note chord.
3. Reported latency.
4. Aliasing of a bright sawtooth at high pitches.
5. Pitch spread across voices: drift and slop.
6. Filter slope where noise allows it.

Run the same measurements on Optisynth's modules.

## Results (19 November 2026, final: whole init presets, requests 67–70)

**What went wrong first, and the correction.**
- An Arturia Audio Unit does not start neutral. Its saved state
  (`fullState["Controller State"]`) holds a full preset archive, and on load
  SEM V2 contained the factory **"Acid Binge"** (an arpeggiated sequence patch)
  and Prophet V3 **"Fascination"** (a pad). The name "Untitled" hid this.
- The first two passes layered guessed overrides on top of those patches, and
  probed the overrides by trial, so the SEM results were meaningless.
- Each instrument now loads an init preset **whole**: `render_au` puts
  Arturia's own preset file into the AU state in memory, then reads the preset
  name back. There are no parameter overrides, so routing, voice count, play
  mode and MIDI channel are the preset's own:
  - SEM V2: "Init Voice 1 (Sawtooth Wave)", 8 voices.
  - Prophet V3: "Default", 5 voices like the original. An 8-note chord sounds
    only the newest 5.
- Reproduce with `tools/reference_render/run_comparison.sh` inside a workload
  job.

**Aliasing.** Init sawtooth patches as saved. Inharmonic energy from 20 Hz to
16 kHz relative to harmonic energy, with energy below the fundamental in
brackets; lower is cleaner.

| Note | SEM V2 init | Prophet V3 Default | Optisynth (Normal 2×) |
| --- | --- | --- | --- |
| C4 | −46.3 (−69.3) | −38.5 (−61.2) | −46.0 (−74.2) |
| C6 | −44.3 (−75.8) | −43.6 (−61.8) | −43.5 (−62.1) |
| C7 | −41.2 (−71.1) | **−48.3** (−65.9) | −40.7 (**−59.5**) |

What this shows:
- Optisynth is level with SEM V2 on overall inharmonic energy.
- At C7, Optisynth leaves the most folded energy below the fundamental: SEM is
  about 12 dB cleaner there, Prophet about 6 dB.
- Optisynth's Draft and High quality levels differ from Normal by under 1 dB,
  so the cause is the oscillator's PolyBLEP, not the filter.

**CPU.** 10 s chords at each instrument's own voice count, effects as saved:

| | Voices | Share of one core | Per voice |
| --- | --- | --- | --- |
| SEM V2 init | 8 | 3.6% | 0.45% |
| Prophet V3 Default | 5 | 4.0% | 0.81% |
| Optisynth OB voice (2 VCOs, filter at 2×, 2 envelopes, VCA) | 8 / 5 | 6.0% / 4.2% | 0.75% / 0.84% |

Optisynth costs about the same per voice as Prophet V3, and about 1.7× SEM V2.
The init voices differ in content, so this is a guide, not like-for-like.

**Pitch spread.** Both init presets measure 0.17 cents range, which is the
analysis resolution: no per-voice variation by default. Optisynth at drift
0.45 spans 3.98 cents.

**Latency.** All three report 32 samples (0.67 ms). Optisynth's filter adds 0,
15 or 22 samples at Draft, Normal or High.

**Side effects.**
- `lsof` showed no sockets at mid-run or at the end of each render.
- No audio or MIDI device was opened.
- Preset loading changes only the in-memory plugin state. Arturia's own
  preference and database files were updated today, either by its standalone
  app or by plugin loads; no preset files changed.

## Next actions

1. Oscillator band-limiting. Target: energy below the fundamental at C7 down
   from −59.5 dB to SEM's ≈ −71 dB, and inharmonic energy at C7 at or below
   −48 dB.
2. Profile the Optisynth voice. SEM shows about 0.45% per voice is achievable.
