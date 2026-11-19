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

## Results (19 November 2026, revised after request 67)

**Method:**
- Every Arturia render starts from an init preset: SEM V2 "Init Voice 1
  (Sawtooth Wave)" and Prophet V3 "Default".
- The Audio Units load an "Untitled" state and expose only generic
  program-change presets. So `tools/reference_render/arturia_preset.py`
  converts each preset file into parameter settings. It matched 156 of SEM's
  201 parameters and 162 of Prophet's 194 (Prophet through its XML
  internal-to-display name map). Preset state that is not exposed as a
  parameter cannot be applied this way.
- Test overrides on top of the presets:
  - SEM: VCF Mode = 0, chorus and delay off.
  - Prophet: VS mixer path to the Pro5 filter off, chorus and delay off.
- Renders are offline at 48 kHz. Reproduce with
  `tools/reference_render/run_comparison.sh` inside a workload job.
- The first pass, made from the default "Untitled" state, is superseded: it
  left unknown settings active.

**Validity:**
- Prophet V3 Default produces a clean sawtooth (harmonics −6.8, −9.3, −11.8 dB
  …), so its results are valid.
- SEM V2 does **not** run normally in this host. From the init preset, a held
  note is a quiet click train with a flat spectrum (RMS 0.0025), and an 8-note
  chord is completely silent.
- Possible causes, none confirmed: licence or demo state, or behaviour specific
  to this host. Yan can check by playing SEM V2 in Arturia's own app or a DAW.
  SEM figures are excluded below.

**Aliasing.** Inharmonic energy, 20 Hz–16 kHz, relative to harmonic energy;
lower is cleaner.

| Note | Prophet V3 (Default) | Optisynth, Draft 1× | Normal 2× | High 4× |
| --- | --- | --- | --- | --- |
| C4 (262 Hz) | −38.4 dB | −45.8 | −46.0 | −46.2 |
| C6 (1047 Hz) | −43.4 | −43.1 | −43.5 | −43.9 |
| C7 (2093 Hz) | **−48.3** | −40.3 | −40.7 | −41.1 |

Energy folded below the fundamental at C7: Prophet −65.9 dB, Optisynth −59.5 dB.

What this shows:
- Optisynth is cleaner at C4, level at C6, and about 7.5 dB worse at C7.
- Filter oversampling changes aliasing by under 1 dB; the remaining aliasing
  comes from the oscillator's two-sample PolyBLEP. The fix is oscillator
  band-limiting, not more filter quality.

**CPU.** 8-note chord, 10 s, effects off, share of one core:
- Prophet V3 Default: 6.2%.
- Optisynth OB voice (two VCOs, filter at 2×, two envelopes, VCA): 6.2%.

They are equal. The earlier SEM V2 figure (2.0%) came from an unverified state;
from the init preset SEM was silent (0.28%). Neither is a valid voice cost.

**Pitch spread.** Eight successive A4 notes, init presets unchanged:
- Prophet V3 Default: 0.17 cents range, which is the analysis resolution. Its
  Default has Voices Detune at 0.
- Optisynth at drift 0.45: 3.98 cents range (σ 1.5), deliberately
  hardware-like.

**Latency:**
- Prophet V3 and Matrix-12 V2 report 32 samples (0.67 ms); SEM reports the same.
- Optisynth's filter adds 0, 15 or 22 samples at Draft, Normal or High.

**Network and side effects:**
- `lsof` showed no open sockets in the test process at mid-run or at the end
  (point-in-time checks only).
- No audio or MIDI device was opened.

## Next actions from these results

1. Oscillator anti-aliasing: aim for at least −48 dB inharmonic at C7.
2. Profile the Optisynth voice. CPU now matches Prophet V3; no cheaper valid
   reference exists yet.
3. SEM V2: Yan to confirm it plays normally in Arturia's own app. If it does,
   investigate offline-host behaviour before repeating SEM measurements.
