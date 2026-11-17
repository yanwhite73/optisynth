# Handoff: partials synth (Claude → Codex), 9 November 2026

Recipient: Codex, thread "VST project — Claude handoff" (01a1bff2-d7d6-7441-ac66-3c732e4b3093).
From: Claude Code session a270d5c1-3fd4-4b5e-a340-cc819f9e215c. Full transcript, if detail is
needed: `~/.claude/projects/-Users-yanwhite-Documents-Music-docs-Code/a270d5c1-3fd4-4b5e-a340-cc819f9e215c.jsonl`.

## What the project is

`partials` is Yan's own synthesiser. It starts as close as possible to LinPlug Spectral's sound
engine (spectral additive oscillators with a spectral filter curve), then grows to Yan's taste,
for example by combining spectra and samples (CronoX ideas). It is not a marketed clone.
Yan's latest instruction was "keep building", autonomously.

- Code: `/Users/yanwhite/Documents/Music docs/Code/partials` (C++17 engine, JUCE 8.0.8 plugin).
- Reverse-engineering material: `/Users/yanwhite/Documents/Music docs/Code/reverse-engineer`.
- Read first: `partials/docs/design.md` (goal, code boundary, milestones, a log of every step)
  and `reverse-engineer/docs/spectral_architecture.md` (what is known of the original, with
  measured sections). Also `reverse-engineer/docs/reference_host.md` for the demo-mode host.

## Rules that must keep holding (agreed with Yan)

1. **Reimplementation boundary.** Reimplement documented and measured behaviour in our own
   code. Never port, translate or recompile LinPlug's decompiled code. Small behavioural
   constants (limits, formula constants, measured values) may be reproduced. Larger data
   tables (unison/phase tables, wave ROM, lookup tables) are not lifted.
2. **Demo noise.** The original runs in demo mode and adds noise. Never bypass, patch,
   subtract, filter or edit around it; no serials, keygens or patches. Yan asked once whether
   it could be silenced and that was declined. Demo renders happen only in our sandboxed
   offline host: `sandbox-exec -f reverse-engineer/tools/refhost/no_network.sb` with
   `reverse-engineer/tools/refhost/build/spectral_refhost` (Intel, under Rosetta). Never load
   the original into Live or any other host. Analysis works through the noise (harmonic peaks).
3. **Host safety** (Yan: "I dont want to crash the host"). The audio thread never allocates,
   locks or touches files. Stress-test before anything reaches Live. The AU is **not**
   installed: installing it and running `auval` was offered and Yan has not answered. Do not
   install without asking.
4. **Ableton Live.** Do not launch, drive or quit Live; Live checks are Yan's.
5. **Workload launcher.** Builds, tests and renders go through `~/.local/bin/agent-workload`
   (`--kind build|test`, `--wait`, `--seconds`). Jobs end when the turn ends: keep runs short.
6. **Git.** Nothing is committed (`partials` is a nested repo with no commits; the workspace
   repo ignores it). Do not commit unless Yan asks. Third-party presets, binaries and banks
   stay out of git.
7. **Communication.** Yan prefers one question at a time, full paths and URLs written out, and
   they/them pronouns.
8. A stale folder `partials/plugin/Builds/PartialsStressMacOSX` is left for Yan to delete (a
   safety check blocked its removal; do not work around that).

## Current state (verified 8–9 November)

- **Engine.** Spectral oscillators with the measured filter mapping, cross-modulation, LFOs,
  24-slot matrix, unison, arpeggiator, envelopes, resonance and LP+ all match the original
  within measurement error (see the measured sections of the architecture doc).
- **Effects.** All 13 types exist (`src/core/effects.cpp`). The six added in this session
  (filter, compressor, crusher, gator, EQ, wah) are first versions; the measurements below
  show what to change.
- **Importer** (`tools/import_linplug.py`) maps the six; the wet migration rule is new.
  `presets-imported/` was re-imported (2,645 converted, 0 skipped).
- **Tests.** `make test`: 192 checks, 0 failures. `make asan`: clean. Built 8 November.
- **Plugin.** Last built 8 November 20:12, before the resonance, limiter, envelope and
  effect changes, so it needs a rebuild. The editor label still says "7 effects (no
  arpeggiator yet)" and is out of date. The stress test (`plugin/Tests/StressTest.jucer`,
  `PartialsStress DIR N [ABC]`) has not been run on the new code.
- **A/B.** `partials/renders/ab_compare.sh` then reads `tools/ab_levels.py` output. Loud
  presets are within about 3 dB; edt_Slider_Lead is +6 dB and 10–20 dB too bright from h4 up
  (a voice filtering issue, not the effects); quiet presets are dominated by demo noise.

## Measured this session, not yet implemented (details in the architecture doc section
"Effects, output stage and volume, measured")

1. **Main Vol after the effects.** Today `engine.cpp:233` puts `patch.mainVolumeDb` into each
   oscillator's `baseGain`. Move it to the output stage after the effect chain (keep the
   per-voice Main Volume matrix modulation). To keep effect input levels equal to the
   original's, also move the ×1.9275 calibration (`engine.cpp:726`) from the output stage into
   the voice gain. Net linear level must not change: re-run the unison and level tests.
2. **Compressor.** Add full automatic make-up gain −T(1 − 1/R), a hard knee, and a detector
   that reads a steady sine 2.44 dB under its peak (provisional: a peak follower with a
   −2.2 dB offset fits sines; the law for other crest factors is unknown). Test against the
   measured table (for example 4:1, threshold −20, in −6.5 → out +0.2 dB at the effect).
3. **Output stage.** Replace the hard limiter at 2.0 with a soft compressor/limiter that
   matches the table in the doc (clean to 0 dBFS, about +5.8 dB at +17.5 in, h3 near
   −33 dB). One fitting form: an envelope-driven gain stage plus a soft shaper above 1.0
   (b ≈ 0.15–0.3). Attack and release are unmeasured; choose and mark them.
4. **Filter effect saturation.** Replace ours (it boosts up to +11 dB) with a level-independent
   shaper (c·sign(x)|x|^p fits round 1). First analyse round 2 (10/35/75 % and the Main Vol
   pair in `renders/probe_fx2/FIL_sat*`).
5. **Gator.** Clock from the host transport: add a song-position input (PPQ) to the engine,
   fed from `PluginProcessor` via the playhead, and derive the step from it; free-run when
   stopped. Closed-step level at wet w: (1 − w)² so far. Smoothing: one-pole, about 2 ms at
   0.25, 4 ms at 0.5, 132 ms at 1.0. Analyse round 2 before fixing the law. The synced
   filter-effect LFO probably follows the transport too (not measured).
6. **Not yet analysed (rendered in `reverse-engineer/renders/probe_fx2/`):** crusher (bits,
   auto volume, divider modes and fractional divider), filter types and cutoff LFO depth
   (`FIL_type_*`, `FIL_lfo_*`, reference `FIL_rich_ref`), gator smoothing/wet extras.
   Extend `reverse-engineer/tools/probe_fx_analyse.py` (`PROBE_DIR=probe_fx2`); it has the
   compressor report, and the session used inline scripts for the rest (envelope by
   quadrature demodulation at the C3 fundamental).

## Next steps, in order

1. Analyse the round 2 probes listed above and add the results to the architecture doc.
2. Implement items 1–5, each with unit tests against the measured numbers; `make test` and
   `make asan`.
3. Re-render the probes' partials side (`sh reverse-engineer/tools/probe_render.sh
   reverse-engineer/renders/probe_fx2`) and compare; re-run `partials/renders/ab_compare.sh`.
4. Re-import presets if the importer changes:
   `python3 -I tools/import_linplug.py --batch "../reverse-engineer/corpus/backup-LinPlug/Spectral" presets-imported`.
5. Regenerate the JUCE projects if sources were added (Projucer `--resave`), rebuild the AU and
   Standalone (xcodebuild, ad-hoc signing `CODE_SIGN_IDENTITY=-`), update the editor label.
6. Stress tests, each under 10 minutes: ASan/UBSan phases ABC with about 20 presets; TSan
   phases AC with about 8; Release.
7. Update `docs/design.md` and the architecture doc. Then the backlog: Precision drift, glide
   (MRG0) and pitch-bend ranges; arp Random, Chord, As Played, note advance and legato; the
   resonance skirts; the slider lead brightness; the provisional matrix scalings.
8. Ask Yan (one question) whether to install the AU and run `auval`, when a build is ready.

## Files changed in this session

- `partials/src/core/effects.cpp`, `effects.h`: six new effects; EQ Nyquist clamp.
- `partials/tests/test_core.cpp`: `testDynamicsAndFilterEffects` (50 checks).
- `partials/tools/import_linplug.py`: six field maps, enum/pattern/vol/lin handling, wet rule.
- `partials/tools/ab_levels.py`, `partials/renders/ab_compare.sh`: new A/B level comparison.
- `partials/presets-imported/`: re-imported.
- `reverse-engineer/tools/probe_fx.py`, `probe_fx_analyse.py`: new probes and analysis.
- `reverse-engineer/tools/probe_render.sh`: per-probe start/length/duration, tempo 120.
- `reverse-engineer/docs/spectral_architecture.md`, `partials/docs/design.md`: new sections.
