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

## Results (19 November 2026)

**Method:**
- Reproduce with `tools/reference_render/run_comparison.sh`, inside a workload job.
- Offline, 48 kHz. Arturia plugins were rendered with `render_au` in 512-frame
  blocks; Optisynth with `optisynth_render` through the real Runtime.
- The tests use a bare sawtooth voice with the filter fully open and no
  modulation or effects. Settings files are in
  `tools/reference_render/settings/`.
- Measurements come from `tools/reference_render/analyse.py`. Output is in
  `build/reference/comparison-output.txt` (not committed).

**Validity:**
- Prophet V3: a clean sawtooth (harmonics fall about 6 dB per octave), so
  these measurements are valid.
- SEM V2: **not valid.** No combination of its bipolar mix controls produced a
  sawtooth: six variants were probed. The default patch's other routing could
  not be identified without the current manual, which was unavailable (expired
  TLS certificate). SEM results below are therefore limited to CPU and spread.

**Aliasing.** Inharmonic energy, 20 Hz–16 kHz, relative to harmonic energy;
lower is cleaner.

| Note | Prophet V3 | Optisynth, Draft 1× | Normal 2× | High 4× |
| --- | --- | --- | --- | --- |
| C4 (262 Hz) | −39.9 dB | −45.8 | −46.0 | −46.2 |
| C6 (1047 Hz) | −40.6 | −43.1 | −43.5 | −43.9 |
| C7 (2093 Hz) | **−49.1** | −40.3 | −40.7 | −41.1 |

Energy folded below the fundamental at C7: Prophet V3 −62.7 dB, Optisynth
−59.5 dB.

What this shows:
- Optisynth is cleaner at low and mid pitches, and worse at the top octave.
- Filter oversampling changes aliasing by under 1 dB. The aliasing comes from
  the oscillator's two-sample PolyBLEP, not the filter. The fix is better
  oscillator band-limiting (a wider BLEP kernel or an oversampled oscillator),
  not more filter quality.

**CPU.** 8-note chord, 10 s, effects off, share of one core:
- SEM V2: 2.0% (default-patch content not verified, see above).
- Prophet V3: 6.7%.
- Optisynth OB voice (two VCOs, filter at 2×, two envelopes, VCA): 6.2%.

SEM V2 shows that a much cheaper voice is possible. Optisynth spends
about 0.77% per voice. Candidates: per-sample `exp2`/`tan` calls, parameter
smoothing loops, the FIR halfband, and module call overhead. Profile before
changing anything.

**Pitch spread.** Eight successive A4 notes, round-robin voices:
- SEM V2: 0.17 cents range, which is the analysis resolution. Effectively no
  per-voice tuning difference.
- Prophet V3 at default Voices Detune 0.164: 0.35 cents range around a common
  +3 cent offset. Its slop is not a per-voice spread here.
- Optisynth at drift 0.45: 3.98 cents range (σ 1.5). This is deliberately
  hardware-like; set drift lower for Arturia-like stability.

**Latency:**
- All three Arturia instruments report 32 samples (0.67 ms).
- Optisynth's filter adds 0, 15 or 22 samples at Draft, Normal or High.
- As a plugin, Optisynth would also need to adapt its fixed 24-sample quantum
  to host blocks.

**Network and side effects:**
- `lsof` showed no open sockets in the test process, mid-run or at the end of
  each render. These are point-in-time checks only.
- No audio or MIDI device was opened, and no plugin settings were deliberately
  changed. A plugin's own preference writes were not audited.

## Next actions from these results

1. Oscillator anti-aliasing: aim for at least −49 dB inharmonic at C7.
2. Profile the voice and reduce its cost toward the SEM V2 figure.
3. Configure SEM V2 properly once its current manual is readable, then repeat
   its sound measurements.
