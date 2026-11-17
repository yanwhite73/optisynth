# partials: design

Working codename, easy to rename. Started 8 November 2026.

## Goal

An original synthesiser for Yan that starts as close as possible to LinPlug Spectral's sound
engine, then grows in Yan's direction. The first idea is to combine additive spectra with
samples, in the spirit of CronoX's generators.

- **Not a clone for sale.** The LinPlug name, presets, built-in waves and artwork do not ship.
- **Yan's own `.fxp` presets** are imported for personal use and as the fidelity test set.

## Code boundary (agreed 8 November)

- **Allowed.** The engine follows the architecture and behaviour documented in
  `../reverse-engineer/docs/spectral_architecture.md`: table layout, band-limit tiers, filter
  levels, formulas and parameter meanings. Those are ideas and facts.
- **Not allowed.** No LinPlug code is translated, recompiled or copied.
- **Constants.** Small behavioural constants that the architecture doc records are reproduced
  on purpose, because they define the sound. Examples: the 21 band-limit tier limits, parameter
  ranges, the ÷ 4.04 band limit, and the filter level formula.
- **Data tables.** Larger data tables are not lifted from the binary: the unison detune and
  phase tables, the factory wave content, the sine and volume lookup tables. We design or
  compute our own and say so.
- **Licensing code** has no counterpart here at all.

## Shape

```
src/core/      engine, no framework dependency (C++17)
  spectrum     257-entry spectra and the 241-point filter curve, in dB; normalising
  table_bank   pre-rendered single-cycle tables: 217 filter levels x 21 band-limit tiers
  oscillator   per-voice playback: phase, symmetry warp, tier choice, level and A/B crossfade
  envelope     AHDSFR envelope (M1: provisional segment shapes)
  patch        our own patch model and text format (stable keys, versioned)
  voice, engine
src/cli/       partials-render: patch + notes -> WAV
tools/         import_linplug.py: Spectral .fxp -> .ptch, using ../reverse-engineer/tools
tests/         unit tests, run by `make test`
```

- **Plugin later.** JUCE (`~/JUCE`, 8.0.8) is added only at the plugin milestone, after its
  README and licence are read and recorded in `documents/claude_code_tooling.md`.
- **Our own patch format.** It is plain text, one `key value…` line per parameter, with a
  version line. Keys are stable machine names; display text is never the key. That fixes the
  weakness noted in `linplug_findings.md`.

## Milestones and what proves each

| # | Scope | Done when |
|---|---|---|
| M1 | Spectra, filter curve, table bank (levels and tiers), oscillator playback with filter-level and A/B crossfade, symmetry, tuning, key tracking, filter envelope depth, a provisional AHDSFR, the importer and the CLI renderer | Unit tests pass: a 1-harmonic spectrum gives a sine; no tier holds harmonics above its limit; a flat curve leaves levels unchanged; a one-point curve passes the expected harmonic at the expected level; the tier choice keeps partials under sample rate ÷ 4.04. Yan's presets render without error, and Yan judges the renders by ear |
| M2 | Faithful envelopes (curve shapes), resonance band-pass, LP+ stages, Precision drift, unison spread, X-Mod, LFOs, matrix, glide, arpeggiator | Each behaviour has a unit test against the documented formula; Yan's ear test |
| M3 | The 13 effects, routing into FX slots, Master EQ, Limiter | Impulse and sine tests per effect; Yan's ear test |
| M4 | JUCE plugin, AU and VST3, native arm64, with an importer for `.fxp` | It loads in Live (Yan checks); every corpus preset imports with a mapping report |
| M5+ | Yan's direction: sample generators beside spectra, analyse-audio-to-spectrum, and so on | Decided with Yan |

## Fidelity without reference renders

Clean renders of the original are not available. The copy is unlicensed and runs in demo
mode, and its demo noise is not circumvented. Fidelity is therefore checked in three ways:

1. Unit tests against the documented formulas.
2. Yan's ear.
3. Yan's own old bounces of tracks that used their Spectral sounds, if any exist.

## Known deviations in M1

These are provisional and listed so they are not mistaken for fidelity:

- **Envelope segment curves** are our own until M2 reads the original's shapes.
- **No resonance, LP+, unison, X-Mod, Precision drift, LFOs or matrix yet.**
- **Oscillator Gain's soft curve** is not applied. Volumes are linear dB gains.
- **No effects.** The output is the dry oscillator sum.
- **Our additions.** A 5 Hz DC blocker sits on the output, added on 8 November because
  symmetry warps leave DC offsets that thump. The CLI also normalises renders to −1 dBFS
  unless given `--raw`.

## Log

### 8 November: M1 built

- **Tests.** The engine core, importer and renderer are built. `make test` passes 90 checks,
  covering every M1 criterion in the milestone table.
- **Corpus.** All 2,648 Spectral presets convert to `.ptch`. Twelve share a file name with
  another preset, so a batch into one folder keeps only one of each name.
- **Renders.** Yan's five presets in `Sound Banks/Yan`, plus four factory presets, render in
  `renders/m1/` for listening. Each plays C3 for 2.5 s, then G3 for 2 s.
  - Pitches were checked by spectrum.
  - Partials stay below sample rate ÷ 4.04.
- **Value conventions found while importing** (recorded in `tools/import_linplug.py`):
  - **Volume parameters** are stored in 0.2 dB steps. Stored −30 is the −6.0 dB Spectral shows.
  - **Fil Volume off.** Fil Volume is declared −54 to +12 dB with an "off" flag. Presets store
    values just below −54 dB, read as off (inferred).
  - **Main Vol.** It runs from −24 to +12 dB. Presets use up to +12.
  - **Envelope order.** ME05 instances follow Spectral's host order: Amp 1–4, Cut 1–4,
    Mod 1–3, Main.
- **Two copies of "YW 1 note love"** exist in the backup, one in `_Spectral/` and one in
  `Sound Banks/Yan/`, and they differ.
- **Loudness is not calibrated.** Peaks range from −43 to +9 dBFS. The original's gain curve,
  effects and limiter are not in M1, and its table scaling is unconfirmed.
- **Some presets cannot sound right yet.** Presets driven by the arpeggiator, LFOs or the
  matrix (for example "YW ark pulse minus res") need M2.

### Next: M2

The order is chosen for audible impact on Yan's presets:

1. Resonance and LP+, read from the documented behaviour.
2. The original envelope curve shapes.
3. Unison spread and X-Mod.
4. LFOs, the matrix and glide.
5. The arpeggiator.
6. Precision drift.

### 8 November: playable plugin brought forward (Yan: renders "don't tell me much")

- **Built.** `plugin/` builds an AU instrument and a Standalone app with JUCE 8.0.8, native
  arm64, ad-hoc signed. They are not installed.
  - **Editor:** preset browser (a folder of `.ptch`; default `presets-imported/`), Output level,
    Auto level, and an on-screen keyboard.
  - **MIDI:** notes, sustain pedal, All Notes Off and All Sound Off.
- **Host safety.** Yan: "I don't want to crash the host". The rules are at the top of
  `plugin/Source/PluginProcessor.h`:
  - The audio thread never allocates, waits on a lock or touches files.
  - Presets load on a background thread and swap in under a short lock.
  - The output is clamped to ±2.0, and a NaN guard sits in the engine.
  - Patch values are sanitised on load.
- **Stress test.** `plugin/Tests/StressTest.cpp` runs the real processor code. It passed in
  three builds:
  - **Address and undefined-behaviour checkers:** 0 errors.
  - **Thread-race detector:** 0 warnings.
  - **Optimised build:** no bad samples.
  - **What it covers:**
    - 80 preset loads during playback, some superseded mid-load;
    - random MIDI and block sizes of 1–8192;
    - sample rates of 22.05–192 kHz;
    - 64-note chords;
    - state round trips;
    - destroy-while-loading;
    - garbage state, and missing or damaged files.
- **CPU, optimised build, one core.** The worst block at 44.1 or 48 kHz used 34–57 % of its
  budget, during a 32-voice chord. At 96 kHz the worst case reached 2.0× and at 192 kHz 5.4×.
  So the prototype is for 44.1 or 48 kHz until the voice loop is optimised.
- **Auto level.** On load the plugin plays C3 offline and sets a trim so the peak sits near
  −12 dBFS, capped at −30 to +18 dB. This stands in until loudness is calibrated.

### 8 November: M2a, effects, resonance, unison and routing

The order comes from corpus usage. Across 2,648 presets: effects 96 %, resonance 75 %,
cross-modulation 69 %, LP+ 48 %, and detuned unison in about 40 % of oscillators.

- **Resonance.** A state-variable band-pass at the cutoff frequency, its output added on top of
  the spectral curve.
  - The output gain uses the documented resonance and cubic cutoff law.
  - The damping law is our own: Q from 0.5 to 50.
  - The resonator stays linear. Its contribution is softly bounded by the documented curve.
    (Bounding inside the state distorted the filter; the unit test caught that.)
- **LP+.** 1–4 one-pole stages at the cutoff plus LP Offset; 61 means off.
- **Unison.**
  - Voices from the original's 2/3/4a/4b/5/6 setting. The layouts and phase spread are our
    own.
  - Spread voices sound only when Spread Detune is above 0 (inferred: every oscillator has 2–6
    voices, but most presets spread none).
  - The non-centre voices sit at Spread Volume. The stereo spread uses Osc Stereo.
- **Routing.**
  - Each oscillator splits between Fil Route 1 and 2 by Route Balance (−1 = all to route 1).
  - Slots run 1→6. Each one passes its output to its Fx Route, which must be a later slot or the
    output.
  - A switched-off slot, or one whose type is not built yet, passes audio through.
- **Effects** (`src/core/effects.cpp`). Our own DSP:
  - **reverb:** Freeverb-style, with Jezar's public-domain tunings and two extra all-passes for
    6 diffusors;
  - stereo delay, mod delay, ping-pong, chorus, phaser and flanger.
  - Not built yet, so passed through: filter, compressor, gator, crusher, EQ, wah.
- **Inferred field meanings,** recorded in the importer:
  - Delay "CutLo" is the low-pass corner and "CutHi" the high-pass corner, read from the ranges
    and from preset values.
  - The chorus's trailing later-version field is its Wet.
  - Tempo-sync text becomes beats ("1/8" = 0.5, "*" dotted, "T" triplet).
- **Tests.** 131 unit checks, plain and under the address and undefined-behaviour checkers.
- **Stress test** (plugin code). It passed under the address and undefined-behaviour checkers,
  under the thread-race checker, and in the optimised build.
  - **Bug it found:** effects were given a tempo before their memory existed. The fix is in
    `Engine::load`, with a regression test.
- **CPU, optimised CLI.** 1–2 % of one core with 4 voices; up to 10 % with 32 held voices.
  - The stress test's worst blocks (1.2–1.6× budget) come from its extreme MIDI bursts: up to
    6 note-ons in blocks of 1–17 samples, on a machine busy with other jobs.
- **Open: "YW ark pulse minus res" is about 48 dB quieter than the original.**
  - Its oscillator 1 uses a narrow band-pass curve.
  - With 100 % key tracking, the current level mapping puts that band about 6 semitones below
    the fundamental.
  - Next step: measure the cutoff-to-curve mapping with test presets rendered through the
    original in demo mode. Write narrow-band curves and flat spectra, and see which harmonics
    pass.

### 8 November: filter mapping confirmed by measurement

- **Test.** Nine probe presets rendered through the original (demo mode, sandboxed host) and
  through partials. They are in `../reverse-engineer/renders/probe_filter/`, made by
  `../reverse-engineer/tools/probe_filter.py`.
- **Result.** Both pass the same harmonic in every case, and the neighbouring-harmonic leaks
  match to the dB. The cutoff law, key tracking, curve centre and interpolation are right.
- **The ark-pulse gap is elsewhere.** The prime suspect is filter cross-modulation (Fil XMod),
  most likely the unidentified ±48-level term in the filter position.

### 8 November: cross-modulation, LFOs and matrix (measured)

- **Cross-modulation.** AM, FM and PM on the oscillators; FM on the filters.
  - Depths come from probe measurements of the original.
  - The source is taken after the modulator's amp envelope and before its volume.
  - Oscillators render sample by sample across all four. Modulators that render later in the
    sample are read one sample late, so feedback stays causal.
- **LFOs.** Five per voice, all nine waves, and our own shapes for "Impuls" and "Peak".
  - Symmetry warps the cycle; delay, attack, phase, polarity and smoothing are supported.
  - Tempo sync; free-run mode shares the engine clock.
  - They update every 16 samples, as in the original.
- **Matrix.** 24 slots, resolved to routes at load time, so note-on parses no strings.
  - **Sources:** LFOs, mod wheel, aftertouch, velocity, note, pitch wheel ±, CC 16–19, the
    mod/amp/cut/main envelopes, alternate, random and constant.
  - **Measured scalings:** pitch, cutoff, wave mix, pan and volume.
  - **Provisional scalings,** marked in the code: symmetry, LFO speed, matrix depth and note.
  - Unsupported slots (arpeggiator sources, envelope-time and effect destinations) are counted
    and skipped.
- **Pan law.** Now linear, as measured.
- **The plugin** passes mod wheel, aftertouch, pitch wheel and CC 16–19 to the engine.
- **"YW ark pulse minus res"** rose by 30 dB once the LFO swept its narrow band-pass. It is
  still about 19 dB under the original. Unison loudness is the next suspect: partials
  normalises voices, and the original may sum them.
- **Stress test.** It now takes a phase selection: `PartialsStress DIR N [ABC]`.
  - Under the thread-race checker, run phases A and C (loading and destruction alongside
    playback); phase B is single-threaded and too slow there.
  - **Results:** the address/undefined-behaviour build, the race build (phases A and C) and the
    optimised build all pass with 0 reports.
  - **CPU:** 1–2 % of one core with 4 voices, up to 14 % with 32 held.

### 8 November: unison and arpeggiator (measured)

- **Unison** follows the measured layout, levels, stereo and the 5.7 dB output calibration;
  see the architecture doc. All 11 unison probes match within 0.6 dB.
- **Arpeggiator** (`src/core/arp.cpp`).
  - All 11 modes; resolution, length, octaves, swing, velocity blend and note length.
  - Per-step transpose, velocity (rest and hold), length, note advance and legato.
  - Arp Velocity, Transpose and Length are matrix sources. Mod-Only drives only the sources.
  - Gate, swing and mode order were measured and match the original.
  - **Still to measure:** Random, Chord, As Played, note advance, and how legato behaves.

### 8–9 November: all 13 effects, and effect measurements (handed over to Codex)

- **Six more effects** in `src/core/effects.cpp`: filter, compressor, crusher (fractional
  divider), gator, parametric EQ and wah. `makeEffect` now builds all 13 types. Their laws are
  our own first versions; measurements show several need changing (below).
- **Importer.** Field maps for the six; enum text, gator step patterns (16-bit integers, step 1
  in bit 15), alternative fields (crusher Divider, else Unused), Volume fields in 0.2 dB steps.
  Wet migration: a section without the newer 0..100 % wet field reads the older 0..200 % one
  divided by 200 (corpus evidence: new = old / 2 at conversion). This also changes older
  reverb, delay, phaser and flanger sections, which previously imported at double wet.
- **Tests.** 192 checks (50 new: every new effect is finite at extreme settings and at 22.05,
  48 and 192 kHz, plus behaviour checks). They found an unstable EQ above Nyquist at 22.05 kHz,
  now clamped. ASan/UBSan clean.
- **A/B tooling.** `renders/ab_compare.sh` renders the partials side unnormalised and runs
  `tools/ab_levels.py`, which compares overall, per-octave and harmonic-peak levels (the last
  works through the demo noise).
- **Measured on the original** (see the architecture doc, "Effects, output stage and volume"):
  Main Vol comes after the effects; the compressor has full automatic make-up gain; the output
  stage is a soft compressor/limiter towards +6 dBFS, not a hard limiter; the gator follows
  the host transport; the filter effect's saturation is a level-independent shaper that lowers
  the level. Our versions differ on all of these.
- **Handover.** The full state and next steps are in `docs/handoff_claude_to_codex_2026-11-09.md`.

### 9 November: Codex accepts the Claude handoff

- **Request and boundaries.** Continue the handoff in its stated order. Reimplement measured
  behaviour in original code; leave reference demo noise intact. No AU installation or
  validation in a system plugin folder without Yan's approval, no Live control, no commits.
- **Ordered work.** (1) Analyse the existing round-2 WAVs and record reproducible results,
  including noise limits; (2) correct gain placement, compressor make-up, output dynamics,
  filter saturation and transport-driven gator with measured regression tests; (3) rebuild
  the CLI, render only the partials side of the probes, compare levels/unison/A-B and run
  core sanitizers; (4) rebuild the plugin and run the three stress configurations; (5)
  record precisely what is built, tested and still unverified before any install decision.
- **Mechanism and success criteria.** Compare harmonic amplitudes of untouched reference
  recordings with synthetic-model predictions and partials renders. Low-level linear gain
  must survive the Main Vol/calibration relocation. Compressor steady-state levels should
  match the measured table; the output stage must remain finite and bounded on transients.
  Gator timing must follow supplied PPQ, including jumps and split processing blocks, and
  free-run without usable transport. Timing/detector laws not isolated by the probes remain
  explicitly provisional. A noise-dominated measurement is inconclusive, not a passing match.
- **Initial verification state.** Inherited 192 passing core checks and sanitizer result;
  not yet rerun by Codex. Plugin and stress binaries are stale. No installed build or Live
  verification is claimed.
- **Follow-up request.** Yan asked whether the noise could be bypassed for testing. The
  chosen testing approach estimates harmonic amplitudes in the noisy recordings and reports
  uncertainty; it does not change the demo mechanism or produce noise-removed audio. Some
  round-2 quiet probes are noise-dominated and cannot justify DSP changes.
- **Pre-build source review findings.** Core corrections pass 267 checks. JUCE 8.0.8's
  `MidiKeyboardState::processNextMidiBuffer` takes a blocking `ScopedLock` and may grow the
  MIDI buffer; this contradicts the processor's real-time claim. Replace that audio-thread
  call with a bounded single-producer/single-consumer queue for UI keyboard events, and test
  overflow/release behaviour. Generic MIDI controller handling also precedes All Notes Off
  and All Sound Off, making those branches unreachable; move them before generic CC handling.
  Ignore unsupported long MIDI messages without constructing allocating MidiMessage objects.
  These are necessary host-safety corrections before plugin qualification.
- **Transport API evidence.** Read the installed JUCE 8.0.8 `juce_AudioPlayHead.h` and
  https://docs.juce.com/master/classjuce_1_1AudioPlayHead_1_1PositionInfo.html: PPQ describes
  the start of the callback in quarter notes and is independently optional. Tempo/PPQ are
  read only inside processBlock; stopped/missing/invalid PPQ selects free-run. No transport
  control method is used. Existing JUCE tooling audit remains in
  `../documents/claude_code_tooling.md`; README, licence, release notes and relevant sources
  were inspected again. Builds stay local; there is no plugin copy phase.
- **Confirmation preference.** Yan asked for fewer confirmation requests. Continue authorised
  offline work autonomously; group remaining build/check commands into the managed
  `tools/qualify_plugin.sh` runner so sandbox prompts are not repeated per shell redirection.
  The explicitly reserved AU installation/auval decision still needs approval.

### 9 November: measured effect correction and host-safety verification

- **Implemented.** The 5.7 dB calibration now enters the effects with each voice; Main Vol
  scales the chain output, while matrix Main Volume remains per voice. Compressor make-up
  is automatic, with a provisional sine-calibrated detector. Filter saturation follows an
  original power-curve fit to the two-level probe pairs. Output dynamics follow a measured
  steady-state curve and soft shaping, with a separate ±2 protection ceiling. Gator uses
  optional host PPQ, free-runs when stopped/unavailable, and uses the measured squared wet
  law. Its full-Smooth timing remains approximate. The editor now lists the arpeggiator and
  13 effects. The offline renderer supplies a 120 BPM timeline.
- **Host-safety changes.** UI keyboard events enter a fixed 512-entry SPSC queue; overflow
  clears voices instead of risking a stuck note. Host key highlights are mirrored on the
  message thread through atomic flags. The callback no longer calls JUCE's locking keyboard
  helper, and ignores unsupported long MIDI messages before constructing a MidiMessage.
  All Sound Off/All Notes Off now precede generic controller handling. TSan covers the
  keyboard producer, audio consumer and display polling during concurrent preset loads.
- **Core verification complete:** 267 checks, zero failures, normal and ASan/UBSan. The new
  checks cover the measured level tables, transient safety, gain placement, gator timing and
  wet law. Existing routing, unison, matrix and arpeggiator checks also pass.
- **Plugin instrumented verification complete:** ASan/UBSan A/B/C with 20 presets passes;
  TSan A/C with 8 presets passes. Both report zero failures/bad samples and no sanitizer
  errors. The new phase-C checks exercise MIDI panic, UI input/release, queue overflow,
  keyboard display, long SysEx and optional/jumping/invalid host positions. Instrumented
  Debug timings are not real-time performance evidence.
- **Fidelity comparison complete:** all 105 round-2 renders were regenerated on the partials
  side only, plus eleven unison and nine
  preset comparisons. Compressor h1 is within .2 dB; saturation h1 within .05 dB; output h1
  within .04 dB; unison remains within about .6 dB. These probes calibrated the models and
  are not independent unseen validation. See the architecture document for remaining harmonic
  and noise limitations. The reference synth was never loaded and its recordings were not
  altered. Importer source and the full imported preset corpus did not change.
- **Evidence:** `build/effect_comparison.txt`, `build/unison_comparison.txt`,
  `build/preset_comparison.txt`, `build/stress_asan_{build,run}_2026-11-09.log`,
  `build/stress_tsan_{build,run}_2026-11-09.log`. Reusable managed runners:
  `tools/verify_offline.sh` and `tools/qualify_plugin.sh` (no installation/Live control).
- **Release verification complete:** AU and Standalone rebuilt for native arm64; both
  ad-hoc bundle signatures pass `codesign --verify --deep --strict`. Release processor A/B/C
  with 20 presets passes with zero bad samples/failures. Concurrent browsing phase A used
  0.427 of its block budget at the 99.9th percentile, worst 0.932. Extreme phase-B bursts
  exceeded the block budget at several rates, including 44.1/48 kHz; host load also exceeded
  the workload warning threshold during this run. This passes memory/state/output safety,
  **not** a guarantee of glitch-free real-time playback or high-rate performance.
  Logs: `build/plugin_release_build_2026-11-09.log`,
  `build/stress_release_{build,run}_2026-11-09.log`.
- **Installation/Live state:** nothing installed; no `auval` run, no app launched, no Live
  interaction, no commits. Qualified local AU:
  `/Users/yanwhite/Documents/Music docs/Code/partials/plugin/Builds/PartialsMacOSX/build/Release/Partials.component`.
  Matching standalone: same directory, `Partials.app`. **Installation approval:** asked Yan
  whether to install this qualified AU and run `auval`; no approval received yet. Do not
  repeat the question during listening feedback. Keep Live checks user-performed.
- **Remaining sound work, in order:** re-probe the inconclusive filter-type/LFO and divider
  cases with an audible slot-off reference; establish crusher quantisation/auto gain and full
  gator smoothing; validate compressor transients and output harmonics/crest factors;
  continue Precision drift, glide/bend ranges, the unmeasured arp modes, resonance skirts,
  Slider Lead brightness and provisional matrix scalings from the handoff.

### 9 November: first user listening feedback on the standalone build

Yan: “I think its probably close. I tested a few patches to check the name aligns with the
sound”. This is preliminary user-performed listening feedback after receiving the standalone
test app. The individual presets were not named, and no direct original-versus-partials A/B
comparison was reported, so do not mark particular patches or the whole engine as matched.
The assessment supports broader auditioning; retain the measured limitations and remaining
work above. It does not authorise AU installation or agent interaction with Live. The agent
revealed the app in Finder at Yan's request; the user performed the listening check.

### 9 November: “bt 7th movements” interval report

Yan reported that a BT preset with “7th” in its title sounded like a fifth. The corpus has
one matching filename, `pads moving add/bt 7th movements.fxp`, imported as
`presets-imported/bt 7th movements.ptch` (internal name “7th movements bt”). Investigate
the original preset data, imported spectra, tuning and modulation before changing anything:
a preset name alone does not establish the intended interval. Only oscillator 1 is active;
it is tuned down one octave and has two LFO2-to-wave-mix routes, so the harmonic structure
and its movement are the first checks. The played note and duration were not specified.

Read-only source-preset inspection (`build/inspect_7th.py`, report
`build/bt_7th_inspection.txt`) confirms:

- Original oscillator 1 is tuned −12 semitones. Its A spectrum is dominated by harmonics
  3 (0.312 dB) and 6 (−2.908 dB): about a perfect fifth above the played key and its octave.
  B is dominated by harmonic 2 (0 dB), which restores the played key at this tuning.
- Weaker upper partials include harmonic 14 (A, −16.994 dB; a harmonic minor seventh,
  two octaves above the key), harmonic 15 (A, −14.596 dB; approximately a major seventh,
  two octaves above), and harmonic 30 (B, −17.012 dB; approximately a major seventh,
  three octaves above). The title does not imply a dominant seventh interval between
  the two loudest tones; a root/fifth impression is consistent with the original data.
- Original chord memory is off. LFO2 is a positive unipolar pulse synced to 2/1 with
  Smooth 50%; the two 100% wave-mix routes are present in the source, not duplicates
  introduced by the importer.
- All 4,112 imported waveform level/phase entries equal the decoded original within
  text precision. No importer harmonic-index shift was found.

This explains why a fifth can stand out without establishing full audio equivalence.
No original audio A/B was performed for this preset, and filter/modulation differences may
still change the relative prominence of the weaker upper partials. No DSP or preset changes
were made on the strength of its title.

### 9 November: continue sound matching — filter/crusher pass

Yan asked “what's next?” and then “continue”. Continue the agreed sound-matching work:
first resolve the filter/crusher probe uncertainty, then musical controls, heavy-polyphony
performance and VST3 packaging. Existing installation and Live boundaries still apply.

Immediate experiment: render isolated sines at several frequencies and input levels through
the existing sandboxed offline reference host, with a verified slot-off counterpart. Use
fewer/louder harmonics only if needed for moving-cutoff measurements. Retain demo noise and
all original WAVs; measure scalar harmonic level/phase and nearby noise confidence. First
establish the off/on and frequency references; expand to filter types/resonance/LFO and
crusher divider/quantisation only after those references are valid. A successful probe has
at least 20 dB fundamental prominence and repeatable level differences; inconclusive data
does not justify a DSP edit. Add targeted measured-behaviour tests for supported corrections,
then rebuild/test the core and current local plugin artifacts. No installation or commits.

The preflight changed the immediate order: direct/slot-off/wide-open sine references agree,
but dense waveforms expose a missing peak-normalisation step in our oscillator tables.
Independent phase, A/B mix, filter and note-range probes establish the rule: each full
unfiltered A/B waveform is scaled down to peak one, retaining that gain across all filter
levels and band-limit tiers; sub-unity waveforms are not boosted. Added this load-time step
to `table_bank.cpp` and eight measured/invariant checks. 275 core checks pass normally and
under ASan/UBSan. Reference details are in the architecture doc's round-3 section. This
corrects upstream effect drive; subsequent filter/crusher measurements still use isolated
sines to avoid conflating the two mechanisms. No plugin delivery claimed until rebuilt.

Filter/crusher follow-up implemented: filter cutoff now uses the measured display/pi
frequency, unnormalised band-pass gain, a sine-calibrated resonance curve and a triangle
starting at its midpoint; reset restores phase. Crusher AVG now continuously smooths with
alpha=1/divider instead of box averaging/holding. Added 29 checks: **304 pass** normally
and with ASan/UBSan. New comparisons reuse the reference WAVs without changing them:
AVG within 0.08 dB, filter sweep fundamentals within 0.25 dB, independent cutoff checks
within 0.6 dB. Filter resonance remains level-dependent in the original and approximate
here. Low-bit quantisation/automatic gain remains unresolved; no speculative change made.

The nine-preset comparison is mixed: Slider Lead is now about −5.7 dB harmonic-power
difference (previously +5.7), Seeanemone −11.4 dB. Verified component corrections can expose
other compensating errors, so do not call this a universal fidelity improvement. Exact next
sound task: isolate oscillator/filter/FX gain in those presets, while retaining the confirmed
normalisation. Reports are `build/preset_comparison_fx3_final.txt` and
`build/fx3_{filters,crusher,filter_check}_verification.txt`. Source and probe details are in
the architecture doc.

**Local build qualification complete:** `tools/qualify_fx3.sh` rebuilt the AU and Standalone
Release bundles and ran processor phases A/B/C with 20 presets under ASan/UBSan and Release.
Both passed with zero failures/bad samples; no sanitizer errors reported. Bundle signatures
passed and hashes are in `build/fx3_release_sha256.txt`. Core checks remain 304/0, normal and
ASan/UBSan. No concurrency logic changed, so the earlier TSan result was not rerun for this
DSP-only pass. Release browsing had 99.9th-percentile/worst budget ratios .389/.528; extreme
phase B still exceeded budget (44.1 kHz 9.132, 48 kHz 1.548, 192 kHz 26.344), and the
supervisor reported heavy host load. These results qualify safety, not glitch-free extreme
polyphony. Logs: `build/fx3_stress_{asan,release}_{build,run}.log` and
`build/fx3_plugin_release_build.log`. The same local `Partials.app` now contains this build;
an already-running app must be quit/reopened by Yan to pick it up. No installation, app
launch, Live interaction or commits occurred.

A final compatibility check compared stored DSP Versions 0.974/1.072/1.21: isolated sine,
four-harmonic and filter probes give the same original result. The older version field in
Slider Lead/Seeanemone therefore does not explain their remaining gain mismatch for these
mechanisms. Six reference WAVs and their report are in `renders/probe_fx3/legacy_*` under
the reverse-engineering project. No additional DSP change was needed after qualification.

### 9 November: named character mismatches — Filth Bass, fn HiHat, Tortured Lead

Yan: “filth bass doesnt sound right / fn hihat doesnt sound like a hihat / tortured lead
doesnt sound tortured”. Prioritise these concrete user-reported cases ahead of the generic
Slider Lead/Seeanemone comparison backlog. Corpus candidates are `ib Filth Bass`, `Filth Bass`,
`BAS Filth Bass`, `fn HiHat`, and `hr Tortured Lead 1/2`. Inspect both lead variants and all
bass aliases so a filename clarification does not block progress. Played notes, velocity,
controller settings and which lead variant were not specified.

Plan: inspect active oscillators, source spectra, cross-modulation, envelopes, matrix and
effects against the raw source fields; compare each distinct patch offline with the original
under the existing reference-host restrictions. Isolate suspected mechanisms with controlled
variants before changing DSP/import mapping. The names and feedback identify regressions to
investigate, not permission to redesign factory sounds to taste. Make supported fixes, add
focused checks, rebuild the standalone/AU and stress-test before delivery. No Live control,
installation or commits.

During this investigation Yan asked whether a patched installer could be inspected. Read-only
inspection of package contents, metadata, documentation and install scripts is authorised;
do not execute the installer or patch. Existing reference-host restrictions remain unchanged.
Locate the package before making claims about its contents.

Raw preset inspection: all three Filth Bass aliases have identical named fields. Filter
envelope modulation is spelled `Env-Depth` in the corpus; the importer only recognised
`Env Depth`, losing velocity response in Filth Bass and both Tortured Lead variants.
Also found a matrix addressing bug: resolved routes discard unused slots, but depth targets
were applied by compacted index. Preserve the original slot index (Tortured Lead 2 targets
slot 7 with several preceding gaps). Added a relocation-invariance regression check.

Next experiment (`probe_character.py`): compare each distinct factory patch, then controlled
sine self-PM and external PM at main-envelope sustain 100/25 percent and oscillator-envelope
sustain 100/25 percent. This distinguishes main-envelope gain from modulation-source gain.
Success is matching spectral ratios/shape, not only output loudness. References remain
untouched, including demo noise; no sound character change based on names alone.

The main-envelope correction is confirmed and implemented; 312 core checks pass normally
and with ASan/UBSan, plus three importer tests. First two filter-tap probes were inconclusive:
constant filter attenuation was normalised away in the original, including an attempted
peak outside the validated central curve window. Reassessed against the existing one-point
filter measurement and table-bank documentation. The next experiment uses a nonconstant
central dip and one-point curve plus direct audible-source controls, so attenuation is
verified before interpreting cross-modulation taps. Simple feedback averaging hypotheses
do not consistently match deep self-PM; no speculative averaging change made.

The revised tap experiment confirms two more corrections: OSC sources precede the spectral
filter; FIL sources include oscillator volume but precede filter volume. The engine now
reads the unfiltered waveform only when an active route needs it and applies oscillator
gain to the FIL tap. **320 core checks pass normally and under ASan/UBSan**, plus three
importer tests. One new test initially incorrectly treated AM amount reduction as equivalent
to source attenuation; corrected it to compare two source attenuations, retaining AM's dry
crossfade. This was a test expectation error, not an AM DSP change.

`migrate_env_depth.py` repaired **552 destinations in 423 local imported patches**, changing
only those destination lines. Originals are retained in `build/env_depth_backup/` outside
the browser folder. Source `.fxp` files are untouched. The direct filter-source controls and
PM sidebands now match the valid reference probes (near-null PM carrier remains ~1 dB off).

Named sounds are **not yet fidelity-qualified**: fn HiHat's early mono high-band fraction
rose from 13.8% to 76.9% (reference 86.7%), but its RMS remains about 12.8 dB low. Filth Bass
still lacks low harmonic energy, and both Tortured Leads retain excess high-frequency
self-modulation energy. These comparisons use fixed notes/velocity and untouched demo-mode
references; noise limits quiet-segment conclusions. Reports: `build/character_*_before.*`,
`build/character_*_verified.*`; four distinct factory references and 37 controlled probes
are in `reverse-engineer/renders/probe_character/`.

Next sound work: validate the promising 2x/low-pass hypothesis on independent notes and
sample rates, measure DC as a scalar, and establish the anti-alias/gain behaviour before
changing the engine. See architecture notes. Simple source/output averaging and long feedback
delays were not a consistent fit. Also still unresolved: filter-curve normalisation outside
the central window, matrix-depth amount law/zero-depth targets, LP+ cutoff modulation, low-bit
crusher gain, and the earlier Slider Lead/Seeanemone backlog. Do not mark these named sounds
fixed simply because the supported component corrections passed safety tests.

Installer inspection awaits a path: the documented TOSHIBA drive is not mounted and the
local filename search found no installer candidate. An asynchronous path question is pending.
No installer or patch was run; licence mechanisms and original reference audio remain
untouched. Installer inspection has not yet occurred.

**Local app qualification complete:** `tools/qualify_character.sh` rebuilt Release AU and
Standalone, verified both bundle signatures, and passed ASan/UBSan and Release processor
stress: general corpus 20 loads through ABC, then the four named presets with 12 loads
through AB. All runs report zero failures/bad samples. Core remains 320/0 normally and under
ASan/UBSan, importer 3/0. Release general browsing 99.9th/worst ratios .338/1.328; named
browsing .316/.323. Extreme-polyphony phase B still overran at several rates (general 48 kHz
2.071x and 192 kHz 30.752x; HiHat 48 kHz .293x and 192 kHz 3.448x), with supervisor warnings
about host load/swap. Do not imply real-time deadline safety from these pass results.
No concurrency implementation changed; no new TSan claim. Logs and hashes:
`build/character_{stress,named}_{asan,release}_run.log`,
`build/character_plugin_release_build.log`, `build/character_release_sha256.txt`.
App SHA256 starts `790809719ba6c7d3`; AU `f6fe7dea354af603`. Yan must reopen the existing
local app to use this binary. No app was launched/restarted, no AU installed, no Live action
or commit made. Managed workload status confirms all owned jobs have exited; unrelated
user services remain available. The named sound-matching requests remain open as above.

### 9 November: backup drive available again

Yan supplied `/Volumes/TOSHIBA 3TB/manual backup/Audio/LinPlug/Spectral`; it is now readable.
It contains Sound Banks, Filtershapes, Waveforms, Modules and SoundIdDb.txt, matching the
library already copied into `reverse-engineer/corpus/backup-LinPlug/Spectral`. This is a
content library, not an installer. The previously noted installer archive is also accessible:
`/Volumes/TOSHIBA 3TB/manual backup/Audio/Installers/LinPlug - Spectral v1.2.7 OS X [R2R][dada]/Spectral v1.2.7`.
Its top-level listing includes `Manual.pdf`, `Spectral template.ksd`, unpacked `CRACKS/`
payload and `CRACKS.7z`; no `.pkg` or `.dmg` was found in the targeted archive listing.
Only directory/file listings were inspected; nothing was executed or installed. The PDF is
now an available documentation source for the remaining feedback investigation. Installer
location is no longer blocked; the previous asynchronous path question is resolved.

### 9 November: archive inspection and newly available manual

Yan supplied the exact installer directory and then suggested Internet Archive, Plugins4Free,
KVR and AudioSex as documentation/archive leads. Completed static inspection, not installation.
The expanded VST and AU executable hashes exactly match our existing 1.2.7/build-36 references.
Both are Intel x86_64/i386 and fail strict signature verification as modified. No conventional
installer was found in the expanded tree; the nested 7z was not independently unpacked.
The included 107-page manual was copied unchanged into the ignored reference-doc cache,
text-extracted and nine relevant pages rendered and visually inspected. It is dated February
2015 for 1.2.3, older than the executable. Full evidence, hashes and source links:
`../../reverse-engineer/docs/spectral_archive_inspection_2026-11-09.md`.

New evidence changes the next investigation order:

1. **Unison and FM mode:** the manual says cross-modulation takes one spread voice, and phase
   zero aligns unison phases. Our taps sum voices and our startup uses fixed phase spacing.
   It also documents Off/HP1/HP2 true-FM modes absent from our importer/engine. Filth Bass has
   raw `Osc FM HP Mode = F100`; the named HiHat and lead presets omit that later field.
   Do not guess its enum mapping or legacy default. Measure these before adopting the earlier
   2x/low-pass self-PM hypothesis. HiHat's spread/self-FM makes it the direct named-preset case.
2. **Zero-depth matrix rows:** manual p100 explicitly starts a vibrato row at zero and
   introduces depth from a wheel row. Our importer and resolver both discard zero amounts,
   and our multiplicative depth law cannot reproduce this. Tortured Lead 2's aftertouch
   vibrato uses such a row. Measure additive scaling, then repair retention and the law
   together with backed-up import migration and controlled/named comparisons.
3. **LP+ and resonance modulation:** LP+ cutoff needs twice the matrix depth for the same
   motion as main cutoff, according to p24. Filth Bass uses this unmapped destination.
   Filter RM means resonance modulation, not ring modulation (p25); repeat the earlier
   no-effect probe with nonzero resonance. Osc Gain and Osc Volume also need separate treatment.

The manual contradicts itself about OSC-envelope/volume modulation (p19 versus p21), and
our controlled envelope measurements contradict p19. Keep measured amp-envelope/Main-envelope
fixes; the manual does not justify reverting them. No manual evidence establishes oversampling.

Web check: LinPlug's historical product/changelog page is readable, but its linked installer
fetches were unsuccessful. KVR's indexed discussion points to Wayback captures, not verified
downloads this turn. Plugins4Free's LinPlug page lists Free Alpha and CM-505, not Spectral.
The Japanese distributor's product page remains another primary documentation source.
No unlock/patch material was used as DSP evidence and no installer was downloaded or executed.

No DSP, imports or app binaries changed in this inspection; no new build/test claim. The
qualified 320-check app above remains current. All named sound complaints and the earlier
backlog remain open. Next action: scoped reference probes for FM-mode/default and single-voice
unison taps, followed by the matrix-depth experiment. Inspection jobs exited normally;
no owned temporary job remains, and unrelated user services are preserved.

### 9 November: continuation after manual review — experiment plan

Yan asked to continue and then asked whether more files need copying from Toshiba. Verify
every non-metadata library file against the local corpus before advising disconnection;
the manual and identical reference binaries are already local. Continue sound work locally.

Use fresh `probe_manual` references through the existing approved offline host. Matrix
experiment: Constant into a pitch row at zero/nonzero depth, with another Constant row
targeting its depth; compare direct equivalent amounts, signs and sparse row numbers.
Success means equal measured pitch for the direct and indirect equivalent, establishing
the law without needing a new MIDI-control host. Unison experiment: compare isolated
source PM and direct-audio controls at one/three voices, spread levels and phases; distinguish
one source voice from summed source sidebands and inspect startup alignment separately.
FM experiment: compare the declared raw Off/F100/D 40 options and legacy-field absence,
using controlled self-FM and the HiHat configuration. Exact settling/level behavior must
support any chosen implementation; a mode label alone is insufficient.

Retain old reference WAVs and noise, measured envelope behavior, installation/Live boundaries
and no commits. No unison/FM implementation change until those probes are interpreted.
Then implement supported corrections, targeted regression tests, backed-up preset migration,
normal and sanitizer checks, local AU/Standalone rebuild and processor qualification.

The held-out self-PM experiment now supports 2x synthesis plus output low-pass: at PM10,
note 72/48k measured -29.82 dB RMS / 4103 Hz centroid; the independent 2x model gives
-29.77 / 4109 (1x: -26.63 / 11521). At note 60/96k: reference -29.54 / 5990,
2x -29.52 / 6164. PM100 levels agree within .3 dB at both cases. Low-level centroids
are noise-contaminated, so do not use them as exact filter fits. More decisively, rendering
our named lead patches at 96k and low-passing to 48k brings Lead 1's early centroid from
6498 to317 Hz (reference298), Lead 2 from2844 to522 (reference558), with levels within
.5/.1 dB. This is a behavioral implementation choice supported by independent tests,
not proof of the original's internal architecture. No proprietary DSP code was translated.

Implement 2x only for patches using audio-rate cross-modulation or matrix routes that can
activate it. Render voices at the doubled rate, decimate each oscillator send before the
effects, and retain effects/arp/transport at host rate. Use our generated 63-tap windowed-sinc
filter, fixed storage, reset its history on engine reset/load, and verify irregular render
sizes and held-out RMS values. The causal filter adds about 15 output samples of delay;
this is a small internal filter delay, not a claim of sample-aligned null matching. CPU cost
and stress qualification must be rechecked before delivering the rebuilt app.

Filth isolation leaves the main error in oscillator 1's modulation/resonance. A slow
sine-to-pitch test shows NORMAL/CUT+ matching but UNI+ starting near zero instead of
our +0.5. UNI- also starts near zero. Early slopes differ from NORMAL, so neither a
simple DC shift nor a guessed squared wave is justified. Next controlled test changes
rate, initial phase and matrix amount independently and observes complete cycles;
success is one polarity/phase law explaining all cases. Keep the qualified app unchanged
until that mechanism is resolved. Original WAVs retain their noise throughout.

Full-cycle and nonzero-phase follow-ups separate the mechanism: UNI- is `(sine-1)/2`,
not the negative of UNI+. At exactly phase zero, both unipolar sine offsets start at
zero and reach their normal +/-0.5 in about 1/24 cycle; later cycles do not repeat
the ramp. Rates .05/.5 Hz and amounts 10/20 support this timing. Starting at even 1%
phase skips the ramp. Implement an independent approximate offset slew of 24 times
the cycle rate, scoped to sine/zero-phase/non-free-running startup, plus the normal
negative-unipolar formula. This does not establish the original internal interpolation
algorithm; small curvature/interpolation differences remain. Verify audio-derived pitch
cases, rerender Filth and the other complaints, then repeat final qualification.

#### Completed qualification before the final LFO change

The matrix, single-voice OSC tap, spread phase and 2x synthesis changes passed 340 core
checks normally and under ASan/UBSan, plus four importer checks. AU and Standalone Release
rebuilt successfully. Broad processor stress (20 preset loads, ABC) and named-preset
stress (12 loads cycling the four complaint presets, AB) passed in Release and ASan/UBSan,
with zero bad samples. Both bundle signatures verified. Logs and executable SHA-256 values
are archived in `build/manual_qualification_before_lfo/`; no AU installation, auval, app launch, Live action
or commit was performed. The build is local arm64.

This proves offline numerical/lifecycle checks, not guaranteed real-time performance.
Broad Release phase A: 99.9th-percentile load .453, worst 1.522 times the audio deadline.
The 48k extreme-note phase B reaches 2.710; 192k reaches 22.240. Named phase A is .633
at the 99.9th percentile and .973 worst, with named phase B still reaching 3.696 at 192k.
Sanitized timings are much slower and not a performance measurement. High-rate/polyphonic
cost remains an open issue; TSan was not rerun in this continuation.

TOSHIBA is no longer needed for this round: all 2,771 non-metadata library files, totaling
24,235,661 bytes, match the local corpus by size and SHA-256; no missing/changed files.
The manual and matching executables are cached locally. Verification record:
`../../reverse-engineer/corpus/reference-docs/spectral-1.2.7/library_copy_check.json`.

#### Final LFO correction and comparison

An initial build queue expired under high host load. After a fresh status showed recovery
below the admission limit, the source change was reapplied and built through the launcher.
All 349 core checks now pass normally and under ASan/UBSan, including nine new scalar
pitch regressions; all four importer tests pass. No guard was bypassed or another owner's
job stopped. The temporary pending patch is superseded by the active implementation.

Rerendered only our sides of `lfo_origin`, `lfo_cycle`, `lfo_start`, `filth_split` and
the four named baselines using `tools/verify_lfo.sh`. In the slow-LFO comparison, UNI+
peak pitch now measures 309.50 Hz against 309.42 original (previously 357.53); UNI- is 236.77
against 236.97 (previously 191.46). The nine time-window pitch checks are
within 2 Hz of the references. This supports the approximate startup law, not exact
waveform equivalence for every shape, symmetry or free-run mode.

Named attack window .11-.18s, fixed notes and velocity 100:

| Preset | Original RMS / centroid | Final Partials RMS / centroid |
|---|---|---|
| Filth Bass | -14.46 dB / 94.3 Hz | -13.0 dB / 162.5 Hz |
| fn HiHat | -30.33 dB / 10597 Hz | -28.9 dB / 8533 Hz |
| Tortured Lead 1 | -5.80 dB / 298 Hz | -6.2 dB / 317 Hz |
| Tortured Lead 2 | -12.44 dB / 558 Hz | -12.2 dB / 535 Hz |

Filth's level error drops from about 3.8 to 1.5 dB, but its early centroid now overshoots;
do not call the tone matched. Other named results are unchanged by this last LFO fix.
Original demo noise limits interpretation of quiet/late spectra. Full report:
`build/lfo_named_verified.json`. The older `probe_character.py` h1..5 report samples
harmonics of the played MIDI note; Filth's oscillators are transposed -24, so those bins
are not its actual first five harmonics. Use the reported RMS/centroid or adjust the
analysis frequency before interpreting its fundamental/harmonic ratios.

Final qualification through `tools/qualify_manual.sh` completed successfully. Current
AU and Standalone Release contain the LFO correction and all preceding changes. All
349 core checks pass normally and with ASan/UBSan; four importer tests pass; broad 20/ABC
and named 12/AB processor stress pass in both sanitized and Release builds with zero
bad samples. Both bundle signatures verify. Current logs/hashes are in
`build/manual_qualification/`; the 340-check predecessor logs are preserved separately.

Release deadline ratios: broad phase A 99.9th=.534, worst=.588; named phase A 99.9th=1.584,
worst=2.259. Broad extreme-note phase B reaches 1.668 at 48k and 28.424 at 192k; named
HiHat phase B reaches .973 at 48k and 4.296 at 192k. Host-load warnings occurred during
qualification. These are observed stress timings, not evidence of a specific LFO-related
performance regression; CPU spikes/real-time overruns remain unresolved.

Current executable SHA-256:

- Standalone: `89a2939fe8839191518d7fb9fd498072d0501155b594b5ce9a325aca5d3a83de`
- AU: `02e562d3750e471049f546d71f2c2e1c87267276f587611a3a5797161c4a2cc3`

Key changed implementation files: `src/core/engine.{h,cpp}`, `tests/test_core.cpp`,
`tools/import_linplug.py`, `tests/test_importer.py`, `tools/migrate_matrix_zero.py`.
Verification/probe scripts and the architecture/reference-host docs record the experiments.
The migration changed 1,191 imported patches and retained their backups. No AU installation,
auval, app launch, Live action or commit was performed. Final workload status confirms
no owned temporary job remains; other owners' jobs/services were preserved.

The final Filth split narrows the next step: with PM disabled, oscillator 1 is within
.35 dB RMS and has centroid 34.1 Hz versus 35.3 original in the .2-.8s window. With PM
enabled it is 1.96 dB loud; bypassing its filter still leaves 1.17 dB, and disabling resonance
leaves 1.20 dB. Thus do not fix this by rescaling resonance or Main Vol globally. Next measure
low-note self-PM at Filth's actual 32.7 Hz fundamental, amounts around 4.6%, with and without
its measured LFO depth and filter. The previous held-out PM probes used 261/523 Hz and do
not establish the low-note feedback behavior. Then isolate the filter interaction.

LP+ mapping, FM-mode settling for HiHat, FIL single-voice modulation, resonance modulation
and the earlier crusher/Slider Lead/Seeanemone backlog remain open. Keep measured fixes
and isolate each remaining mechanism before tuning aggregate preset levels. Existing
no-install/no-Live/no-commit boundaries remain in force.

### 9 November: low-note PM continuation

Yan asked to continue after discussing the offline comparison method. First measure a
single sine at notes 24/48/60 with PM 0/2/4.6/5/7/10 at a stronger, still linear output
level, then distinguish self-PM with one spread voice from three detuned voices. External
cross-modulation's one-voice source does not establish whether self-PM internally uses
that same tap for every spread voice. Separate this before changing a global PM scale.
Compare stereo as well as mono where spread is enabled. Success is a mechanism that
explains independent controlled cases and improves the named preset without sacrificing
the already verified lead/HiHat behavior. Keep all original WAVs/noise untouched, the
349-check app available during research, and the existing host/installation boundaries.

The low-note references preserve shared self-PM feedback as the better model. In the
three-voice/stereo 83 case, reference side RMS is -34.43 dB, shared model -33.87 and
independent model -26.38. A spread gain discrepancy already exists with PM off:
three-voice mono is .79 dB louder than the reference, while a single voice is .26 dB
louder at 32.7 Hz. Old three-voice phase fits also have each amplitude about .02845 versus
the expected .03048 from our half-gain law. Next measure spread volume 0/20/50/100,
different voice counts, and swapped oscillator/Main gains at equal final level. This
distinguishes a spread mix law from compression/order or a global oscillator calibration
error. Yan also asked whether looking at the UI would help: yes, for imported values,
routes and waveform/filter displays; this supplements behavioral measurements. No
original-plugin editor was opened by that discussion.

The spread law is supported by separate 35% tests at 96k/note 48 and 48k/note 72:
additional voices measure .1635/.164 versus our .175, main voices .8097/.8118 versus
our .825. Use an independent empirical full-spread gain of .467, crossfading the main
voice from 1 and others from 0. Two equal-level controls within established volume
ranges (Osc 0/Main -24 and Osc -12/Main -12) agree exactly. Earlier Osc 0/Main -30 and
Osc -24/Main -6 pairs do not preserve the intended gain in the original and were excluded;
do not infer compressor order from those out-of-range/unsupported controls.

PM/symmetry probes expose a second concrete issue. For external PM 5 into a 25/75%
symmetry sine, the reference is -16.07 dB with centroid 1806 Hz. PM after warp (current)
gives -15.66/1696; our independent model with PM before warp gives -16.07/1796. Self-PM
at note 24 also improves: symmetry 75 goes from -18.30 to about -20.65, reference -20.83.
Implement PM phase addition/wrapping before symmetry warping. Flat 50% symmetry is
unchanged. Some self-feedback brightness and high-note levels still differ, so this
establishes the operation order, not a complete feedback model. Validate another
pitch/rate/depth, then compare Filth and the other named presets before qualification.

#### Spread and PM order corrections qualified

The two corrections are implemented in `src/core/engine.cpp`: an empirical .467
full-spread voice gain, and PM phase addition/wrapping before symmetry distortion.
Eight spread amplitude checks and six PM/symmetry checks bring the core to 363 checks,
all passing normally and under ASan/UBSan. Four importer checks also pass. Held-out
external PM at note 48/96k, depth 2, symmetry 35/65 supports the same operation order.
The existing coherent-unison onset check now reflects the measured spread gain.

`tools/verify_bass.sh` rebuilt and rerendered only our sides of the relevant probes.
The named attack window (.11-.18s, fixed notes, velocity 100) now measures:

| Preset | Original RMS / centroid | Partials RMS / centroid |
|---|---|---|
| Filth Bass | -14.46 dB / 94.3 Hz | -13.27 dB / 162.2 Hz |
| fn HiHat | -30.33 dB / 10597 Hz | -29.4 dB / 8532.5 Hz |
| Tortured Lead 1 | -5.80 dB / 298 Hz | -6.2 dB / 317.1 Hz |
| Tortured Lead 2 | -12.44 dB / 558 Hz | -12.3 dB / 541.0 Hz |

These short windows do not establish a full preset match. Filth's held 1-1.5s window
still differs strongly: original -15.7 dB / 114.9 Hz, ours -10.23 dB / 33.0 Hz.
Its isolated oscillator 1 is 1.52 dB loud in .2-.8s; bypassing the filter or turning
resonance off leaves only .21 dB. This now prioritizes the resonance interaction over
another global PM or Main Volume adjustment. Reports: `build/bass_named_verified.json`,
`build/bass_spread_fit_verified.txt`, and the retained `probe_manual` originals.

`tools/qualify_manual.sh build/bass_qualification` completed. AU and Standalone Release
rebuilt; signatures verified; broad 20/ABC and named 12/AB processor stress pass in
Release and ASan/UBSan, with zero bad samples. No AU installation, auval, app launch,
Live action or commit was performed. TSan was not rerun.

Real-time deadline overruns remain: broad Release phase A 99.9th .755, worst 12.326;
named phase A .455 / .531. Broad phase B reaches 2.020 at 48k and 29.360 at 192k;
named HiHat reaches 2.186 at 48k and 4.016 at 96k. These are measured under shared host
load, not a guarantee of real-time performance or proof of a regression from these fixes.
The previous 349-check logs are preserved in `build/manual_qualification/`.

Current executable SHA-256 (`build/bass_qualification/release_sha256.txt`):

- Standalone: `f2b8255e17ceae6cfa2c0bb2074769f1c5258101f320abe291fd983aab5aea80`
- AU: `56f1e4a18bb592e438c5a8e7d6fbcba4d736554907b432374e81309feb475812`

#### Original-code inspection clarification and Claude review request

Yan asked why Codex could not see what the VST code does. The earlier explanation was
misleading: the original source is unavailable, but the compiled instructions and saved
Ghidra decompilation are inspectable. The agreed boundary permits studying behavior;
it prohibits translating/copying proprietary routines and lifting large data tables.
The architecture document already used static analysis, and it should continue to guide
targeted measurements rather than treating all DSP as an opaque black box.

Located the existing `scratchpad/decomp/renderNX.c` from Claude's handoff session.
Initial reinspection shows nonlinear resonance state feedback, whereas our current
resonator is linear internally and softens its added output. LP routing and gain placement
also warrant checking. No resonance implementation change has been made on this evidence.
Yan then explicitly requested confirmation with Claude and its installed reverse-engineering
skill. A read-only Claude Code review ran through the shared launcher; its request
is in `build/claude_filter_review/`. Raw tool events containing decompiler excerpts stay in
`/private/tmp/codex-partials-claude-review-01a1bff2/`.

The [completed independent review](../../reverse-engineer/docs/claude_filter_review_2026-11-09.md)
confirms in-loop resonance saturation, PM-before-symmetry and the LP+ routing difference
in the decompiler output. It corrects the placement of the output level/envelope factors
in Codex's initial diagram. The 2x loop has strong static support, but the caller's rate
and decimator connection still need tracing. No assembly or new runtime evidence was
produced by this review. Claude invoked the installed skill and used only Read/Grep/Skill;
the session had no MCP servers, shell or editing tools, and no permission denials.

The architecture document reconciles the verdict rather than accepting all of its wording:
lookup tables/callers can be studied statically; they were simply not fully traced here.
Filth's spread branch feeds resonance after LP+, so its resonant peak cannot be assumed
constant when LP+ changes. Next measure a controlled Osc Volume/Fil Volume drive sweep
with LP+ off, then one-voice versus spread LP+ routing; use a verified pre-resonator gain
route if neither volume control changes the resonance ratio. Hold modulation constant
for these mechanism tests and retain the existing full-preset references separately.
No new resonance DSP was written. The qualified build remains the 363-check version above.

### 9 November: Internet Archive installer discovery

Yan supplied `Spectral Installer 127.exe` and `.dmg` from Downloads, then asked to log
<https://www.kvraudio.com/forum/viewtopic.php?t=555483>. The thread, original download
metadata and installer hashes are recorded in
`../../reverse-engineer/docs/spectral_internet_archive_installers_2026-11-09.md`.
Inspect these statically and compare the Mac plugin's identity/signatures with the
modified reference before selecting a reference binary for further filter experiments.
No installer execution, AU installation or Live interaction is implied.

The Mac package now validates as LinPlug-signed with a trusted 28 May 2015 timestamp.
Both plugin executable hashes differ from our old references. Strict all-architecture
signature checks pass for verification copies containing byte-identical Contents files,
excluding only legacy root Finder metadata/xattrs. Old reference copies fail the same
procedure. All originals are preserved; no binary/signature was modified or re-signed.
The initial sandboxed verification errors were misleading; use macOS security services
for signature checks. Both installers are cached locally, and the manual hash matches.

No new plugin was run and no DSP changed. Before the pending resonance probes, compare
a small set of fresh archive-binary reference renders with the old renders, recording
binary hashes and retaining demo noise. Keep this separate from AU installation or Live.

### 9 November: Windows installer payload inspection

Yan asked whether the Windows installer had actually been examined. Only the outer PE
and provenance had been checked; the plugin DLLs were not yet unpacked. Continue with
static extraction now. The PE overlay at 569344 begins with Clickteam's `wwgT)H` marker;
the loader version resource is 2.0.0.43 (not the synth version). Read the installed reverse-
engineering skill's packaging workflow and the complete cicdec 3.0.1 source/docs before
choosing a decoder. Use its documented file-record layouts in a bounded Python reader,
with the BSD attribution retained. No .NET tool, installer or DLL execution is required.

Success: validate block/record boundaries and decompressed lengths, inventory all members,
extract only DLLs and documentation to an isolated ignored cache, then inspect PE architecture,
exports, version and symbol/debug evidence. Report decompilation readiness separately from
actually tracing Windows DSP. No licensing/demo-state alteration, installation, Live action
or commit is authorized by this inspection.

The first strict inventory rejected the initially selected v40 layout. A raw-record
diagnostic also rejected that layout, so stop using it: the first record is a 16-bit
75-byte node of type 2 (uninstaller), followed by a 111-byte type-0 node whose filename
starts at byte 68. Those fields match cicdec's documented v30 layout, including a
32-bit index and three FILETIME fields. Update the reader from that evidence; preserve
both file-list variants rather than relying on cicdec's single-list CLI behavior.

Completed: extracted both DLLs with exact sizes, valid zlib checksums and no unused bytes.
They identify as Spectral 1.2.7, PE32 x86 and PE32+ x86-64. Each variant has 1,092 preset
records; neither includes a separate manual or PDB. The x64 DLL names a missing PDB,
both retain `GenSpectral_c` RTTI, and both export only `main` with no COFF symbols.
Full hashes and evidence are in the installer inspection document. No Windows DSP has
yet been decompiled or run; no synth code, installed plugin or qualified app changed.

New user request: ask actual Claude whether comparing both Windows and Mac versions
helps decompilation or reduces inference. Prepare a restricted read-only review using
the installed reverse-engineering skill, saved Mac decompilation and Windows metadata;
ask for concrete next steps and limits rather than assuming identical cross-platform DSP.

Claude review completed, with skill use and only Read/Grep/Glob/Skill calls. Report plus
Codex reconciliation: `../../reverse-engineer/docs/claude_windows_mac_review_2026-11-09.md`.
The verified Mac remains the primary target; native `nm` confirms named renderNX/prepProc
methods survive. Windows x64/x86 add alternate compiled views, not recovered source or
proof of equivalent DSP. A useful correction: Downsample_c appears among effect objects;
remove its mere presence as evidence of oscillator decimation. Twice-length loops/half
phase increments and audio fits still motivate tracing the actual caller/output consumer.

Next: compare the verified archive Mac renderNX/prepProc/setters with old Toshiba-derived
analysis, check the key assembly branches, trace controls and rate path, then use Windows
for remaining ambiguities and hash-labelled offline renders for drive/preset verification.
The Windows DLLs remain inert cached files; no DSP change, installation or commit occurred.

### 9 November: is Downsample_c the bitcrusher?

Yan suggested that Downsample_c might refer to bitcrushing. The manual confirms the
Crusher includes both bit-depth and sample-rate reduction. Direct saved-code inspection
maps those user controls to Crush6_c, while Downsample_c declares only a Version field.
The preceding Claude review's user-effect interpretation was therefore premature.
Saved assembly shows Downsample_c consuming pairs of float samples, filtering both and
calling its output method once per channel per pair: evidence favoring internal 2:1
conversion. Its generator connection and behavior in the verified archive remain pending.
Details/corrections are recorded in the architecture doc and Claude report addendum.
No plugin ran and no implementation changed during this clarification.

### 9 November: continue with verified DSP and rate-path tracing

Yan said to continue. First compare selected DSP symbol spans in the verified archive
Mac VST against the old reference, recording addresses/sizes/hashes rather than copying
proprietary code. Then import the archive x86_64 slice into an isolated scratch Ghidra
project with the reviewed headless workflow and export only relevant generator, instrument,
buffer and downsample routines to scratch. Trace the generator's length/rate and the
Downsample input/output connections. Success requires a caller/buffer path, not a class
name. Keep observations about the old build separate until byte comparison supports them.

After the static path is resolved, run the scoped archive-reference comparison and gain
probes through the existing sandboxed offline host. Preserve demo noise and raw references.
Only change our independently written resonance model when the verified topology and
measurements identify a specific mismatch; then run core/sanitizer tests, held-out renders
and plugin qualification. No AU install, Live interaction or commit is part of this work.

Completed static/reference milestone: all 55 selected DSP symbol spans match the old
reference byte-for-byte, and fresh Ghidra analysis traces the generator through seven
double-length DS buffers and seven 2:1 converters into normal-length FX buffers. This
resolves the Downsample/Crusher ambiguity. Seven fresh archive-reference WAVs also match
their preserved old references exactly, including demo noise; this establishes equivalence
for those cases, not for the entire binary. Evidence is in the architecture document.

The first 15 gain/resonance probes completed without a DSP change. Removing Filth's PM
leaves an almost pure 32.7 Hz sine, far below the 221 Hz resonator. Quiet RMS values are
inflated by demo noise, whereas the strong fundamental follows the -6 dB gain steps.
Do not infer compression from those RMS values or fit resonance from this weak excitation.
Next use an independently specified 16-harmonic saw, flat spectral curve, C3 fundamental
and 523.25 Hz cutoff (fourth harmonic), with Osc Volume -12/0/+12, resonance 0/44/75/100,
LP+ off/0-semitone and single/spread comparisons. Main -18 and Fil Volume -12 keep output
headroom; repeat two cases at Fil Volume -24 to distinguish input drive from output gain.
Success is a measured level-dependent resonant/dry ratio or LP routing difference above
the measured nearby noise floor. Raw demo output remains intact.

Completed: 20 rich-signal probes, bringing this archive-reference batch to 42 renders.
Dry fourth harmonics agree within 0.08 dB. With resonance 75, the original's boost falls
from 17.86 dB at Osc -12 to 2.50 dB at Osc 0; Partials stays at 21.61 dB. The matched-dry-
gain pair (Osc -12 / Fil -12 versus Osc 0 / Fil -24) differs by 15.35 dB in the original
and effectively zero in Partials. This confirms a drive-dependent filter mismatch above
the noise floor, consistent with the verified nonlinear feedback and gain placement.
Reference peaks stay below 0.387. Separate LP+ single/spread probes also completed.

`summarise_archive_reference.py` validates all 42 reference hashes, preset hashes and frame
counts and records numerical targets. One initial summary assertion assumed an explicit
sample-rate key in the old manifest; it was corrected to the runner's 48 kHz default and
passed. No reference WAV was altered. Full results and traced addresses are in the final
section of `../../reverse-engineer/docs/spectral_architecture.md`.

Next concrete action: prototype our own nonlinear feedback resonator offline, with UI
Osc Volume and amp envelope before its input, Fil Volume after it, and the separate LP+
paths. Validate against the matched-dry-gain pair and complete drive sweep; then measure
held-out cutoffs/waves/attacks before integrating. Keep current qualified app until this
candidate passes core/sanitizer tests, named-preset renders, processor stress and CPU
checks. No DSP source, importer, presets-imported, installed plugin or app changed in this
research batch. The earlier 363-check qualification remains historical, not a new run.

### 9 November: independent nonlinear resonator prototype

Yan asked to continue. Build a small offline model before changing the engine: retain
our trapezoidal two-integrator structure, add a cubic loss inside the band feedback,
and solve the resulting monotonic scalar equation with bounded Newton iterations.
This is an independently derived discretisation, not a translation of the original's
explicit state update or saturation curve. Fit a few damping/drive/tuning constants to
prominent harmonics from the existing rich probes. Keep the matched-output-gain pair
and LP+/spread cases as checks, then measure different cutoffs/waves as held-out evidence.
Integrate only if it improves the measured error materially without losing dry gain;
then qualify normal/sanitizer tests, named renders, CPU and processor stress before
rebuilding the test app. Original noise, installation and Live boundaries are unchanged.

### 9 November: user requests the full existing Spectral UI

Yan wants controls in the app so they can tweak presets and detect flawed sound-model
assumptions by ear. Clarification: "just recreate the existing ui for now". Use Spectral's
manual screenshots and documented control groupings as the layout reference; do not
invent a different dashboard. Implement our own JUCE rendering/assets, preserve familiar
labels/positions, and expose controls that actually affect Partials. Retain the preset
browser and restore/save workflow. Unsupported synthesis behavior must remain distinguishable
from working controls. Complete the nonlinear model's tests alongside this UI work, then
qualify one combined app update. No original plugin GUI or Live driving is needed.

Filter progress before UI work: the frozen independent cubic-loss model fits its nine
training cases at 0.188 dB RMS across measured harmonics, and held-out saw cutoffs at
0.190/0.284 dB. Sine fundamentals/odd harmonics agree closely, but small even harmonics
remain absent; the full unweighted dB metric exposes that rather than proving a complete
match. Integrated engine comparisons complete: Filth early RMS error +0.60 dB, HiHat
+0.88 dB, Lead 1 -0.41 dB, Lead 2 +0.10 dB, isolated Filth oscillator 1 +0.13 dB.
Existing 363 core checks and four importer tests pass. New regression and sanitizer/
processor checks are still required before delivery; the qualified app is not yet rebuilt.

Additional UI request: Caps Lock enables MacBook typing keys as a piano keyboard, with
Z/X octave selection. Limit note input to the focused Partials editor, keep ordinary text
entry intact, release typing notes when Caps Lock/focus changes, and expose current octave
and enabled state. This does not authorize global keyboard capture or driving Live.

### 9–10 November: nonlinear resonator and native editing UI delivered

Implemented the independently derived cubic-loss, trapezoidal resonator described above.
Oscillator gain and amp envelope drive its input; filter volume follows it. The fitted
cutoff/damping/mix constants and the single/spread LP+ branches are in `src/core/engine.cpp`.
Active resonance uses our existing 2x path. The implementation does not translate the
original explicit integrator/saturator or copy a lookup table. Prototype, frozen validation
and 45 integrated comparisons live under `build/resonator_fit/` and
`build/resonator_comparison/`; see the matching architecture entry for error limits.

Replaced the small preset-only editor with our own JUCE rendering of Spectral's main
layout: oscillator/filter top row, pale envelope/LFO panels around a central modulation
matrix, arpeggiator and effects below, preset/master controls in the footer. The official
manual page 13 is the arrangement reference; no original UI bitmap assets were reused.
All four oscillator/filter pairs, A/B harmonic amplitude/phase and filter-curve drawing,
amp/cut/mod/main envelopes, five LFOs, all 24 matrix rows, all 32 arp steps, six effect slots
and all 13 effect types can be edited. Matrix/arp use two pages. Click a waveform or filter
display to open its spectral editor. Save As writes a .ptch; Restore returns to the last
loaded/saved baseline. Complete patch edits and that baseline are embedded in host state.
Output and Auto Level remain the only host automation parameters; this update does not
expose every patch field as an AU automation parameter.

Caps Lock enables local typing notes: A W S E D F T G Y H U J K O L P ; ' chromatically from
MIDI 48 (C3); Z/X shifts octave. It reads Caps Lock modifier state, without an event tap or
global capture. Notes only start when Partials has focus and no text field/command modifier
is active. Focus loss, Caps Lock off, key release and editor destruction release notes owned
by the typing keyboard. The onscreen piano and MIDI input remain available. Physical Caps
Lock/focus behavior still needs Yan's direct audition; automated checks verify the note
ownership, octave edges and release logic without generating system keyboard events.

Scalar edits reuse immutable waveform/filter tables. A worker builds the new engine and
transfers held-note phases, envelope stages, LFO state, transport and unchanged effect tails
under the existing audio try-lock. Existing audio never waits for the edit. Changes are
coalesced during dragging (about 120 ms) and submitted immediately at drag end. Changed
FX parameters rebuild that effect, so its own tail can restart; table rebuilding may also
add editing latency. Auto-level trim stays at the loaded preset's measured value, allowing
relative gain changes to remain audible. New roundtrip checks caught and prevented stale
embedded patch duplication on repeated host saves.

Qualification (`build/ui_qualification/`): 393 core checks and four importer tests pass,
including ASan/UBSan. The new core checks verify sample-identical untouched voice/effect
state transfer, cache reuse/invalidation, patch serialization and typing-key lifecycle.
ASan/UBSan and Release processor phases A/B/C/D pass across 20 general presets, with
12 additional A/B loads from the reported named cases. Phase D changes actual editor
sliders, waveform buttons, second matrix/arp pages, FX type/fractional bit depth, verifies
save/restore twice, resizes/snapshots the editor and runs audio concurrently with 60 edits.
No invalid samples or processor test failures. Release browsing deadline ratios:
general 99.9th=.594 / worst=.661; named .584 / .602. Extreme phase B still overruns:
general 48k=1.694 and 192k=22.360; named 48k=.586 and 192k=4.528 worst ratios. Host pressure
warnings occurred. Passing numerical/lifecycle tests does not establish real-time safety
for those extreme cases. App/AU signatures verified; hashes are in `release_sha256.txt`.

The first UI compile exposed a macOS `Component` name collision, corrected with explicit
JUCE qualification. First screenshots exposed UTF-8 passed to JUCE's ASCII constructor,
clipped labels and overlapping spectral-editor headings; corrected before release.
The snapshot writer initially appended PNGs when rerun, so viewers displayed its first
image; it now truncates explicitly. Final sanitizer UI verification reruns on the release
editor source and produces fresh images. No AU install, auval, Live interaction or commit.

Remaining scope: this is a functional recreation of the existing arrangement, not a
pixel-identical skin or a complete Spectral engine. Filter RM is visibly marked not built
and its depth disabled. Glide, Precision, envelope pitch/velocity scaling, original
MIDI-learn/undo workflows and master EQ are not implemented by this editor; EQ remains an
available effect type. Imported unsupported matrix IDs/data remain preserved. Complete
sound equivalence remains unproven, including weak even resonator harmonics and extreme
CPU demand. Next action is Yan's audition/tweaks of the reported presets; use the saved
edited .ptch and the exact control/oscillator/FX slot to isolate any remaining mismatch.

Final UI verification completed 10 November on the same editor/processor source as the
Release app: `final_ui_asan_build.log` and `final_ui_asan_run.log` pass with no JUCE assertions
or sanitizer errors (493 concurrent audio blocks in phase D). Fresh main, spectral and
960-pixel editor captures were visually inspected. The Release app executable is
`7c33a57bd9918ebb6d05297f7078389decb330386375bdffee561d6dafb5d3f5`;
the local AU executable is
`40274ad58e1d091cf8d4b37ca1aa3ec6234101898297c715b9b222eff91a6b70`.
All owned build/test jobs completed and were removed from the workload register.

### 10 November: smoother editing, host parameters and independent backups

Yan reports the current app is sounding pretty good, asks to continue, wants Optimystery
LFO parameter access ultimately, asks whether VST3 is the best format, and wants code
backups independent of GitHub. He explicitly authorizes passing the integration findings
to the Opti session. No publication, commits, installation or Live driving is authorized.

Plan: first add fixed continuous controls with stable IDs and an audio-side smoothing path,
so host LFOs work while the UI is closed without rebuilding tables or resetting voices.
Retain the worker for structural/waveform changes. Preserve same-type effect state across
parameter edits, with focused tail/continuity tests. Audit unsupported imported modulation
usage before choosing the next synthesis feature. Build/test local formats and save a
portable source archive plus restore instructions outside the project tree. Archive only
our source/docs/tests and required independent importer helpers, not original installers,
plugin binaries, presets or reference audio. A local same-disk archive is not off-device
protection; it is ready to copy to separate storage.

VST3 is the proposed primary host format; AU and Standalone remain useful. VST3/AU can
both expose controls to Live, but current Optimystery explicitly skips Plugin devices
(`modulation.js:359`). Integration therefore also needs Opti work; handoff is in
`optimystery_parameter_handoff.md`. Use official VST3/C74/Ableton docs there. Stable parameter
IDs and actual engine response are the acceptance criteria, not a new file extension.

### 10 November: continuous controls and VST3 implemented

The processor now has 62 permanent host parameters: legacy Output/Auto level plus
60 continuous musical controls. Registry: `host_parameters.json`. Fixed arrays of
atomic parameter values feed the existing audio engine without invoking the editor
or worker. Cutoff smooths in log frequency, other values in their physical units,
with an approximately 4 ms time constant at control-block boundaries. Plugin engines
use fixed 2x synthesis to avoid changing sample rate when resonance/XMod is automated;
the offline renderer retains its existing automatic oversampling choice. This has a
CPU cost and does not establish realtime headroom at extreme rates/polyphony.

Native controls use parameter attachments and begin/end gestures. Waveforms, routing,
envelopes and other structural edits retain the coalesced worker path. Same-type
effect edits transfer delay/reverb buffers, oscillator phases and filter/envelope
history while keeping new coefficients; different effect types reset. This preserves
tails but does not claim every coefficient change is click-free. Spectral shape or
large delay-buffer changes may still produce a silent block during the try-lock swap.

State now includes current host-controlled values. Patch serialization uses nine
significant digits to round-trip float values. New host state restores its parameter
values directly, without a second conversion from text; old embedded-patch state
still seeds the new parameters. Ten repeated recalls retain the same control values.
The symmetry knob now displays the full 1–99% range accepted by the engine/API;
the original's narrower base control was 4–96%, so this is a deliberate extension.

Actual local VST3 enumeration: 63 automatable parameters (62 plus wrapper bypass)
and 2,080 non-automatable MIDI proxy parameters. `vst3_parameters.json` records their
numeric IDs by unique name. The MIDI proxies remain because MIDI controllers and
pitch bend need them; they are not modulation candidates. Do not equate these IDs,
JUCE string IDs, Live indexes or runtime DeviceParameter IDs. JUCE 8.0.8 consumes
the final non-MIDI automation point per block: this implementation is not sample
accurate or intended for external audio-rate modulation. Opti's owning session
received the contract and is responsible for its profile/discovery/recall work.

Verification: 485 core checks, four importer tests and core ASan/UBSan pass. Debug
ASan/UBSan and Release processor phases A/B/C/D/E pass across general and named
presets. Phase E drives all 60 additions with the editor closed, verifies no queued
rebuilds, finite/bounded output, preset synchronization and repeated state recall.
Cutoff change on a low-pass fixture moves measured energy from 4.043 to 349.063.
Phase V loads the actual VST3 locally, checks IDs/names/flags, plays MIDI, changes
audio with a host parameter, and recalls its VST3 state. The full run is recorded
under `build/host_qualification`; final D/E/V checks cover the UI range correction.
AU, VST3 and Standalone are local-only builds, with copy phases disabled; no Live
interaction, installation, original-plugin modification or commits occurred.

This shared-machine run produced timing outliers: general Release phase A 99.9th
percentile 13.269x budget; the four named presets 0.981x. Dense/small-block phase B
also overran, including at 48 kHz. Concurrent builds/pressure warnings make these
unsuitable as a clean benchmark, but they do not justify a realtime-performance
claim. CPU optimization and a controlled benchmark remain open.

The missing-feature census is in `feature_priorities.md`/`feature_usage.json`.
Unsupported matrix destinations and envelope scaling have wider preset coverage
than Filter RM alone; that evidence determines the next sound-fidelity work.
`tools/backup_source.py` creates source-only archives with per-file hashes and a
SHA-256 sidecar, verifies all contents, and restores only into a new directory.
Restoring/building the core is independent of GitHub and the original plugin.

### 10 November: user authorizes VST3 installation for Optimystery testing

Yan asked to install the tested VST3 after clarifying that the earlier app link
opens Standalone Partials. Scope: install only Partials.vst3 in
`~/Library/Audio/Plug-Ins/VST3/`, verify the copy, and retain user-performed Live
checks. This does not authorize an AU install or driving/restarting Live.
Preflight found no existing Partials VST3 in the user or system VST3 folder.
The local bundle passes strict signature verification and matches the qualified
executable SHA-256 `dff59ec033e846a8ae95768717bc7d3791a9cce09cabdd857eeb427b33afa716`.

Before the copy completed, Yan interrupted and requested the public name
**Optispectral**. Neither old nor new VST3 was present in the user plugin folder
after the interruption. Continue the authorized installation using Optispectral
0.1.1 after rebuilding and checking its actual host metadata. Public app/plugin
names, bundle filenames and UI branding change; source-folder names, preset/state
format, parameter IDs, manufacturer/plugin codes and bundle identifier remain.
JUCE's VST3 wrapper uses manufacturer/plugin codes with VST2 replacement disabled,
so changing the name need not change its CIDs. Compare old/new actual metadata
and parameter IDs before delivery. Opti's owning session has been asked to add
Optispectral recognition while retaining its legacy product aliases/recall profile.

**Complete:** Optispectral 0.1.1 AU/VST3/Standalone rebuilt and signature-verified;
Release D/E/V passes for UI controls, host automation with no editor, and actual
VST3 sound/state recall. Side-by-side metadata comparison confirms all 62 parameter
entries, 63 VST3 automatable parameter IDs, three class CIDs and the JUCE host UID
are unchanged. The macOS bundle/display names and UI branding are Optispectral.
Correcting only targetName initially left the macOS display name as Partials;
renaming the Projucer project fixed that. The generated project is now
`plugin/Builds/PartialsMacOSX/Optispectral.xcodeproj`; qualification scripts match it.

After the required filesystem permission, VST3 installed at
`~/Library/Audio/Plug-Ins/VST3/Optispectral.vst3`. All six installed bundle files
match the qualified source and strict signature verification passes. Executable
SHA-256: `e1142c3907d0bca1f7876350574088bff0baa3d61177e29f339b510b84e5f2cc`.
No old Partials VST3 was installed, so no prior installed bundle needed replacing.
Reports: `build/name_qualification/identity_check.json`, `install_receipt.json`,
`stress_run.log` and `release_sha256.txt`. AU remains uninstalled; Live was not
driven or restarted. Opti received final metadata and owns its queued alias/recall
update. Actual host discovery, movement and Rack recall remain user checks.

**Opti integration complete (owner report):** Optimystery dev.52 installed at
18:32:08 with exact Optispectral/Partials product aliases, unchanged `partials-v1`
profile/39 targets, and verified cross-name recall using descriptors exported by
actual dev.51. Its qualification record reports 60 Python checks, three Node
suites on source/archive and ten plugin test groups per run; installed files match
the archive. A fresh main dev.52 device activates the alias update, with no extra
restart and no replacement of existing opticapture. This closes the code/install
handoff; actual Live discovery, audible movement and saved-Rack reopen stay open.

### 10 November: native preset export for Opti's Rack builder

Opti's owning session requested an offline `.ptch` -> native `.vstpreset` helper,
starting with `Pad/pads gated/jam Spectral Beauty`. `PartialsStress` now loads the
patch normally (including auto-level), transfers its state through JUCE's inspected
host envelope, and invokes the real VST3 SDK preset writer. Fresh-instance native
recall preserves the complete embedded state, all 62 controls and +5.751705 dB trim;
128,000 stereo frames match fresh direct recall sample-exactly. Source, command,
artifact hash, host-cache synchronization and asynchronous-loader caveats are in
`docs/native_preset_export.md`. No production/DSP/installed bundle changes.

The sample was passed to Opti for byte-exact Comp/Cont extraction into its sample
Rack. Opti owns category-preserving source staging and Rack work; Live acceptance
remains user-performed before full-library installation. The source backup allowlist
now includes the new `plugin/Tests/PresetExport.h` dependency.

**User correction relayed by Opti:** the sample Rack "loads ok but sounds quite
wrong". Yan explicitly places this in plugin sound fidelity, separate from the
preset path. Track `jam Spectral Beauty` as an unresolved sound complaint. The
offline sample-exact comparison proves state transport within our implementation;
it does not compare our sound with the original Spectral. Next sound task: compare
this named patch with the original reference and inspect its enabled/routed
features; do not close it on Rack load or state tests. No fidelity fix in this
export-only update.

Yan accepted Rack loading and authorized Opti's own-folder batch. Opti requested
an explicit state-only export mode so silent presets are retained. Keep all state,
control, bypass and auto-level checks, omit audio acceptance only in the opt-in
mode, and qualify its Comp/Cont identity against the existing audible sample.

**State-only export completed:** `--export-vstpreset --state-only` passes normal
load/auto-level, SDK export/fresh recall, exact state, 62 control and bypass checks
without an audible-reference requirement. Reports explicitly mark audio unverified.
Seven qualification cases pass: both modes reproduce the existing native sample
byte-for-byte; silence exports only in explicit state-only mode; unknown/duplicate
flags and overwrite are rejected. Opti has the qualified helper and owns full-tree
export/installation. The plugin bundle and DSP remain unchanged.

Bulk export exposed a one-ULP change in the duplicated Patch symmetry value during
JUCE recall (`BAS staccato bass`). Saved APVTS parameters and every other field were
unchanged. The exporter now compares exactly with an independent direct recall,
guarding persisted state and non-host Patch fields against any source change.
Schema 2 reports distinguish source equality from recall equality. Eight checks
pass; details/hash in `native_preset_export.md`. No numeric tolerance or DSP change.

11 November: two more presets showed the same normalization effect in persisted
host floats too. Replaced the partial exactness workaround with a common <=1e-6
normalized tolerance for recognized host floats, preserving exact non-host state,
LoadedPatch, metadata, trim and discrete controls. Every native stage is compared
with the original source as well as direct recall to bound total drift. Schema 3
records equality separately from bounded acceptance. All three regressions, prior
sample/refusal checks and 1,227 comparator checks pass; Opti pinned the qualified
helper for its v3 batch. No DSP/install changes. Sound-fidelity complaint stays open.

### 11 November: requested host modulation coverage

Yan reports that only the top section seems to be modulated and asks about effects,
the matrix and envelopes. Opti's owner audited all 39 existing eligible controls
(28 oscillator/filter, 5 LFO rates, 6 FX wet) and reports 311 recent native mapping
acknowledgements covering all of them. The missing controls are absent from the
plugin's host API: envelope segment parameters, 24 matrix amounts and detailed FX
parameters. Existing Filter Env controls set depth, not envelope shape.

Requested next action: assess and propose stable IDs/names/ranges plus DSP and
state migration implications, coordinate with Opti's profile/Rack owner, preserve
all existing IDs and preset recall. This is separate from sound-fidelity work.
Assessment only until the contract and required DSP behavior are specified; no
Live UI actions or claim that mapping acknowledgements prove audible modulation.

Assessment completed in `host_parameter_extension_proposal.md`: proposed stable
IDs/names/ranges for 108 envelope controls, 24 matrix amounts and 71 typed FX
controls per slot. Implementation prerequisites are active-voice envelope/matrix
updates, typed FX setters preserving history, buffer capacity and presence-aware
old-state migration. No additional controls are built or installed yet. Opti owns
the matching profile and Rack configurations; preserve the current 62 controls.

Yan also challenged repeated token-heavy checks after a successful prior run.
Reuse qualified build/export evidence; this assessment requires no rebuild or
corpus rerun. VST3 format support is not the missing-control blocker.

Yan authorized implementation of the missing controls on 11 November: build the
envelope/matrix/typed-FX contract, including old-state migration and realtime DSP,
then qualify the plugin and coordinate its shipped registry with Opti. Preserve
existing controls and CIDs. No Live driving, AU installation or commits.

Implementation range audit: 2,636 imported presets contain values outside several
proposed UI ranges. Expanded the new host ranges to retain those values (including
Filter FX modulation to 320 Hz, delay LP to 22 kHz, HP down to 8 Hz, and zero
compressor attack). Retained existing DSP clamps for chorus/flanger/attack, so
previously clamped presets keep their sound and raw saved values. EQ now realizes
the same static RBJ transfer through TPT integrator state for moving coefficients;
this replaces the unstable direct-form state during extreme control sweeps.

**0.1.2 delivered:** all 558 additions implemented (620 processor controls).
Core/sanitizer runs each pass 1,194 checks; importer four; state comparison 8,127.
Final plugin A/B/D/E/F/V passes, with C already passed; native export/fresh recall
retains 620 controls and sample-exact reference audio. Original IDs are preserved.
Extreme stress exceeded real-time deadlines despite finite/bounded output.

Verified VST3 installed; previous bundle backed up outside search paths.
Details/hash/receipts: host_parameter_extension.md and build/host_extension/.
Opti's owner is integrating relevant Rack subsets, preserving the original 62
configured controls. No Live driving, AU install, commits or corpus re-export.

### 17 November: Fushigi bounced-audio references

Yan supplied `/Users/yanwhite/Music/Bitwig/Projects/fushigi` as a potential source
of original Spectral audio for fidelity evaluation. Initial read-only inspection
found 138 Spectral-named WAV files, including 109 in `synth/bounce`, and a
`synth/spectral.bwproject` containing a saved `LSP1` plugin state and a preset-path
hint for `ambient/dh telling tales.fxp`.

Mechanism: inventory RIFF headers and hashes, deduplicate by file hash, and inspect
the embedded ZIP members of projects in the five folders containing candidates.
Preserve only Spectral state members as data with source hashes. Success means a
reproducible reference inventory with explicit state/audio provenance. A project
co-location or preset-path hint alone does not establish a bounce-to-preset match.
Exact patch, MIDI, tempo, automation and downstream processing must be established
before interpreting differences as synth errors. This intake changes no DSP and
does not open a DAW, load a plugin or modify the supplied projects/audio.

Intake completed with `tools/inventory_fushigi.py`. The 138 files reduce to 113
distinct file hashes (109 synth bounces plus four gold-float bounces), totalling
99.74 minutes of unique audio. All are stereo, 44.1 kHz, 24-bit PCM WAV; duration
range is 2.16–408.83 seconds. The other 25 files are exact copies. All RIFF
headers/frame boundaries validated. No sound-accuracy score is implied.

Across 86 nearby project versions, four embedded `LSP1` occurrences yield three
distinct bank states: `synth/spectral.bwproject`, `synth/legacy cell.bwproject`,
and `gold float/gold float bouncd.bwproject` (also duplicated under `2016`).
Exact banks and the hash inventory are saved in
`build/fushigi_reference_20261117/`. The bank payloads are uncompressed
`SYG0 / SYG1 / TUN0 / PROG` section containers, unlike packed `.fxp` presets;
the initial preset decoder assumption was corrected after inspecting their
headers. Outer sections parse completely. `PROG` contents and Bitwig's MIDI/song
graph have not been decoded. The `dh telling tales` path is a project metadata
hint, not yet a verified current-program or audio match.

Next fidelity step: recover/identify a current program from a saved bank and
establish its matching bounce, MIDI and processing chain before rendering the
same performance through Optispectral. Until matched, these files provide
listening references rather than controlled synth-only A/B fixtures.

Yan authorized continuation, then specifically asked whether the Bitwig projects
can supply the Spectral preset. Prioritize extracting and validating the actual
saved program settings before audio matching. The observed `PROG` body begins
with a four-byte index and the familiar Spectral packed-program stream; validate
its complete decoded section layout and strict schema before conversion.

Program recovery succeeded for all three banks. Their internal saved paths name
`1 note love it bt`, `ark Binary Message` and `ark 5th Dimension`. The generic
Bitwig browser metadata naming `dh telling tales` was stale. Each packed program
is preserved verbatim in a standalone FXP wrapper, its strict section/token
decode re-encodes byte-exactly, and an Optispectral `.ptch` is generated. Named
fields differ from the corresponding corpus presets only in `DSP Version`.
Artifacts and hashes: `build/fushigi_reference_20261117/programs.json`.

MIDI research before implementation: Bitwig's official DAWproject FAQ documents
notes/plugin state export, but native BWPROJECT requires separate decoding.
The primary `jaxter184/bwEdit-Python` README/changelog and complete decoder source
describe big-endian typed object fields, terminated object arrays and references;
its old project support is explicitly incomplete. Consulted as format evidence
only, not installed or executed. Source and field-name references:
https://github.com/jaxter184/bwEdit-Python/blob/master/src/decoder.py and
https://github.com/jaxter184/bwEdit-Python/blob/master/src/lib/luts/names.py .
Observed file header offsets agree with the object root and embedded ZIP start.
Use a bounded read-only decoder, preserve unknown field IDs/types and fail at an
unsupported encoding. Whole-container parsing alone is not proof of note/clip
semantics; validate timing and routing against the referenced audio before A/B.

Yan additionally identified `+ mobilise`, with no known local bounces but likely
heavy Spectral use. Include its project versions and backups in saved-state
recovery; the absence of audio does not prevent recovering embedded programs.

The first strict graph attempt stopped on old inline-name fields. A second attempt
using a newer Bitwig class-group table stopped earlier at the transport object.
Stopped those attempts and reassessed: format 0x75 needs its old grammar, not the
4–6 class table from Wavelength. Local bytes at 9910 show a length-3 inline key
`361` with its own terminated field list, exactly the old decoder's documented
case. Subsequent graph work must implement that explicit old-format construct,
retain version-specific grammar, and require complete body consumption. Meanwhile
bank recovery already works independently and remains the priority for Mobilise.

Mobilise check complete: 171 project versions/backups contain no embedded `LSP1`
bank states. Spot-checking the main, v2 and v3.1 project plugin paths and bank IDs
identifies many `CrX4` (CronoX 4) instances, not Spectral. Record:
`build/mobilise_reference_20261117/inventory.json`. No CronoX binaries were loaded.

The independent old-format graph reader now consumes all bytes up to the ZIP in
the three Spectral projects. Gold float retains a Spectral-only instrument chain,
two identical 12-note MIDI clips at beats 448 and 512 (64 beats each), and bounces
2 and 3 at beats 320 and 576. Tempo is 110 BPM; the bounce source maps 64 beats to
34.90909 seconds without a tempo change. This is a candidate phrase comparison,
not proof that the saved preset created those earlier bounces. The track has mixer
automation and a Presswerk send, so absolute bounce gain/routing also needs care.

Comparison mechanism: recover notes and provenance to JSON, render the saved patch
at 44.1 kHz and 110 BPM, and compare pitch/timing, spectral balance and envelopes.
The CLI previously forced 120 BPM and delivered all note events at block starts.
Add explicit tempo/song position and split rendering at sample-rounded note events
before using it for this comparison. Verify event timing and repeated-pitch release
ordering. This changes the offline CLI only; no DSP, installed VST3 or DAW changes.

Candidate comparison completed: `tools/compare_fushigi_phrase.py` renders all 12
notes for 64 beats (34.90909 s) at 110 BPM. `gold-float-clips.json` retains note
positions, lengths, raw velocities, clip/track node IDs and source project hash.
The MIDI clips are identical; pitches cycle 46, 51, 49. No note expression or
clip automation was found in the selected phrase. Its instrument chain contains
only Spectral. Historic bounce stage and preset-at-bounce remain unproven.

Bounce 3 is the strongest candidate of four: mean pitch-class cosine 0.902,
spectral centroid 561 Hz versus 584 Hz for Optispectral; both place over 99.8%
of spectral power in 200–2000 Hz. These are descriptive features, not an accuracy
percentage. Optispectral measures 4.04 dB lower in whole-phrase stereo RMS and
4.05 dB lower in peak. Envelope correlation is only 0.063; random/free-running
LFO behaviour and unrecovered historical state prevent a waveform-match claim.
Do not change engine gain based on this provisional comparison.

Listening files `listen-original-spectral-bounce-3.wav` and
`listen-optispectral-candidate.wav` have equal stereo RMS and common headroom to
a maximum -3 dBFS peak. Only constant gain is applied; original files remain
untouched. Their hashes and method are in `gold-float-comparison.json` alongside
raw renders, analysis, exact command, renderer and patch hashes. The script can
regenerate listening copies with `--auditions-only` after verifying source hashes.

Verification: offline renderer built successfully; its focused CLI regression
passes (sample-aligned onset, a sub-block-length note, and same-pitch retrigger).
Both listening copies were decoded again to verify equal RMS and peak headroom.
No installed plugin change or Live verification is part of this work.

Next controlled fidelity step: validate this phrase/preset against the approved
original offline reference host, including the saved outer bank globals and
velocity law, or obtain a historical state tied explicitly to bounce 3. Keep
these archival bounces as provisional listening references until that link is
established. Mobilise provides no additional Spectral programs in the inspected
171 versions; its observed LinPlug states are CronoX 4.

### 17 November: proposed modular instrument direction

Yan asked about extending this into a multi-engine instrument for agent-assisted
composition: samples, granular synthesis, JP-8080/Xpander-inspired modes and
Mutable-derived modules. He added physical modelling primarily for percussion,
including samples as excitation or parallel layers, and clarified that individual
engine components should share envelopes/effects and be freely routable. This
is a feasibility/design discussion; no engine implementation or branch created.

Assessment from current source: the framework-independent core, offline renderer,
versioned patch keys, host parameter metadata and separate Effect interface are
reusable. Voice, modulation destination enums and Patch remain fixed around four
Spectral oscillators; they are not a general graph. TableBank precomputes spectral
filter curves into oscillator tables, so that operation cannot directly filter
arbitrary incoming sample audio. General audio filters/resonators need their own
processing modules. Existing high-polyphony CPU limits remain relevant.

Proposed shape: typed audio/control/note connections, independent generators,
processors and modulators, explicit per-voice versus shared/global scope, and a
versioned structured patch with stable node/parameter IDs and sample asset hashes.
An envelope can fan out to multiple destinations; shared effects run after voice
mixing unless explicitly placed inside a voice. Feedback would require an explicit
delay or a separately designed solver. Graph preparation and sample loading stay
off the audio thread, with bounded voice/grain/node counts and reproducible seeds.
Preserve the existing Spectral engine as a compatibility module during extraction.

Agent access should use the same patch model as the editor: inspect modules and
active routing, edit parameters/connections, manage samples, render a phrase, save
variants and undo. VST3 continues as the Live wrapper; an explicit local control
interface and offline renderer provide richer access than host automation alone.
Stable host macros/bindings need their own contract before dynamic graphs ship.

Suggested first musical milestone: sample player plus modal percussion resonator,
shared envelopes/LFOs, mixer/filter/existing FX, and graph save/recall with an
offline audition command. Prove both sample-as-exciter and parallel sample attack,
then add granular and virtual-analogue modules. Hardware-character modes are
tractable; exact model-by-model emulation is a separate measurement commitment.

Primary feasibility references inspected (no third-party code built or run):
- Mutable repository README declares STM32F code MIT, AVR code GPL3, with separate
  derivative-name guidance: https://github.com/pichenettes/eurorack . The inspected
  Plaits modal-engine header carries the MIT notice; dependency/source review is
  still required before adopting any selected implementation.
- Rings manual documents external excitation, modal/string resonators, brightness,
  damping and strike position:
  https://pichenettes.github.io/mutable-instruments-documentation/modules/rings/manual/ .
- Roland JP-8080 owner manual:
  https://static.roland.com/assets/media/pdf/JP-8080_OM.pdf .
- Manufacturer-authored Xpander manual, filter modes page (mirror):
  https://www.manualslib.com/manual/3534905/Oberheim-Xpander.html?page=26 .
