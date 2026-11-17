# Optispectral host-control extension proposal — 11 November 2026

Historical proposal: see [the shipped 0.1.2 implementation](host_parameter_extension.md)
for final ranges, verification and installation.

Status: assessment and proposed contract, **not implemented**. This answers the
Opti owner's request for envelopes, matrix amounts and detailed FX controls.
The existing installed plugin still has 62 processor parameters, of which 39 are
Opti candidates. Opti reports all 39 mapped and 311 acknowledgements. Those results
establish bindings, not that every mapped control audibly affects every preset.
Sound fidelity, including `jam Spectral Beauty`, remains a separate open task.

## Compatibility contract

- Preserve every existing parameter's ID, name, order, range, normalization and
  version hint in `host_parameters.json`, plus all VST3 class IDs and state identity.
  Append new controls after the existing 60 live controls; retain legacy Output
  and Auto level. New controls use JUCE parameter version hint **3**.
- IDs below are proposed permanent JUCE string IDs, not Live indices or numeric
  VST3 IDs. After implementation, export actual hosted IDs and check collisions
  against existing controls, bypass and MIDI proxies before updating Opti.
- Names below are unique, at most 16 characters. No slot's parameter may change
  meaning when its FX type changes. Use type-specific IDs, never generic `param1`.
- Existing `fx1_wet` through `fx6_wet` stay unchanged. Structural settings such as
  FX type, matrix source/destination, pattern bitmasks and sync divisions stay
  outside the automatic continuous-control contract.
- Treat the catalogue as a proposed upper bound, not a requirement to configure
  every control in every Rack: 108 envelope + 24 matrix + 426 typed FX = **558 new
  controls**, potentially **620 processor parameters** in total. Only active,
  implemented and musically appropriate controls become Opti candidates.

## Proposed normalization

All additions are continuous (no forced integer steps). For normalized `n` in
`[0,1]`, use `L`: `min + n*(max-min)`; `G`: `min*(max/min)^n` for strictly positive
frequency/time/ratio/Q ranges; `Z`: `u*((1+max/u)^n-1)` for zero-inclusive times,
where `u=0.001` for seconds and `u=1` for milliseconds. Inverses must round-trip
within the existing normalized tolerance. Existing controls retain their mappings.

These ranges follow current UI/DSP evidence, with differences called out below.
Before publishing them, check existing imported values for outliers: widening a
proposal is preferable to silently clamping an old preset on recall. Defaults
come from the loaded Patch, or the existing DSP defaults when a field is missing.
Normalization, ranges and IDs become immutable only when this API ships.

## Envelopes: 108 controls

Expand each row below over these 12 prefixes, in this order:

| Envelope | ID prefix | Name prefix |
| --- | --- | --- |
| Oscillator amplitude 1–4 | `amp1` … `amp4` | `A1` … `A4` |
| Filter cutoff 1–4 | `cut1` … `cut4` | `C1` … `C4` |
| Modulation 1–3 | `modenv1` … `modenv3` | `M1` … `M3` |
| Main amplitude | `mainenv` | `MainEnv` |

Join ID prefix and suffix with `_`; join name prefix and suffix with a space.
Example: `mainenv_release`, name `MainEnv Release`.

| ID suffix | Name suffix | Physical range | Map |
| --- | --- | --- | --- |
| `attack` | Attack | 0–60 s | Z |
| `hold` | Hold | 0–60 s | Z |
| `decay` | Decay | 0–60 s | Z |
| `sustain` | Sustain | 0–1 | L |
| `fade` | Fade | −60–60 dB/s | L |
| `release` | Release | 0–60 s | Z |
| `attack_curve` | A Curve | 0.05–20 | G |
| `decay_curve` | D Curve | 0.05–20 | G |
| `release_curve` | R Curve | 0.05–20 | G |

Eligibility requires a sounding/routed oscillator, an enabled filter with usable
envelope depth, or a supported matrix route consuming that modulation envelope.
Main-envelope shape controls are eligible; main output gain remains excluded.
Release needs note-offs to be audible; changing attack during a held sustain
must not retrigger the voice. An LFO binding alone cannot prove envelope activity.

Required DSP work:

- `Envelope::updateParams` currently assigns parameters through voice inheritance
  after a rebuild. Add allocation-free updates for active and new voices. Preserve
  stage, current level and note-off state; define phase rebasing/smoothing for
  duration/curve edits, and ramp sustain changes (the current sustain stage simply
  returns its stored level). Never resurrect a completed envelope.
- `fade` is **dB/second** in the DSP; the current editor's seconds label is wrong.
  `fadeCurve` is stored but unused: do not expose it as an audible target.
- Source inspection identifies overflow risk in float `expm1(8.25*(curve-1))`
  near the upper curve range. Use a numerically stable calculation before offering
  that full live range, and qualify finite output at its extremes.

## Matrix: 24 controls

IDs `matrix1_amount` … `matrix24_amount`; names `Matrix 1 Amount` …
`Matrix 24 Amount`; physical range **−1–1**, linear. Append in numeric row order.
Source and destination remain structural strings. Zero amount does not make an
otherwise valid route ineligible: external modulation should be able to activate it.

`resolveMatrix` builds routes with `slotIndex`; each voice currently copies and
reads `ModRoute::amount`. Updating only `Patch::matrix` will not update that copy.
Read/update base depth through the fixed slot index for active and new voices,
preserving existing two-pass `matrix.depthN` modulation and its clamp. No string
resolution or route allocation on the audio thread.

Eligibility requires a supported source and destination, plus Opti's existing
destination policy (do not sneak excluded gain/pan controls in through the matrix).
Incomplete Filter XMod/RM routes are excluded. Preset-derived activity metadata
must be invalidated when the user changes routes; a name-only profile cannot infer
which rows do anything. Until that metadata exists, matrix targeting is explicit
opt-in rather than an assertion that all 24 rows are active.

## Effects: 71 controls per slot, 426 total

For each slot `s=1…6`, ID is `fx{s}_{type}_{key}`. Name is `F{s} {tag} {label}`.
Example: `fx2_filter_cutoff_hz`, name `F2 Flt Cutoff`. Expand slots first, then
types and keys in the table order. Existing wet controls are not duplicated.

`M` means expose for manual automation but exclude from automatic Opti LFO choice.
`B` means blocked on buffer-capacity work before the full range can be offered.
All rows also require the matching enabled FX type and a usable audio/wet route.
Time/rate controls overridden by sync are ineligible while synced. Internal
modulation controls need that modulation enabled and a meaningful depth/rate.

| Type / tag | Key | Label | Physical range | Map | Gate |
| --- | --- | --- | --- | --- | --- |
| reverb / Rv | predelay_ms | PreDelay | 0–2000 ms | Z | B |
| reverb / Rv | time_s | Time | 0.1–50 s | G | |
| reverb / Rv | damping | Damp | 0–0.95 | L | |
| reverb / Rv | mod_depth | Mod Depth | 0–20 | L | B |
| delay_stereo / SD | time_l_ms | Time L | 1–8000 ms | G | B |
| delay_stereo / SD | time_r_ms | Time R | 1–8000 ms | G | B |
| delay_stereo / SD | feedback | Feedback | 0–0.99 | L | |
| delay_stereo / SD | highpass_hz | HP | 20–20480 Hz | G | |
| delay_stereo / SD | lowpass_hz | LP | 20–20480 Hz | G | |
| delay_stereo / SD | stereo | Stereo | −1–1 | L | |
| delay_mod / MD | time_ms | Time | 1–8000 ms | G | B |
| delay_mod / MD | feedback | Feedback | 0–0.99 | L | |
| delay_mod / MD | mod_depth | Mod Depth | 0–1 | L | |
| delay_mod / MD | mod_rate_hz | Mod Rate | 0.01–20 Hz | G | |
| delay_mod / MD | stereo | Stereo | −1–1 | L | |
| delay_mod / MD | highpass_hz | HP | 20–20480 Hz | G | |
| delay_mod / MD | lowpass_hz | LP | 20–20480 Hz | G | |
| delay_pingpong / PD | time_l_ms | Time L | 1–8000 ms | G | B |
| delay_pingpong / PD | time_r_ms | Time R | 1–8000 ms | G | B |
| delay_pingpong / PD | time_c_ms | PreDelay | 0–8000 ms | Z | B |
| delay_pingpong / PD | feedback | Feedback | 0–0.99 | L | |
| delay_pingpong / PD | balance | Balance | 0–1 | L | M |
| delay_pingpong / PD | pan_l | Pan L | −1–1 | L | M |
| delay_pingpong / PD | pan_r | Pan R | −1–1 | L | M |
| chorus / Ch | time_ms | Time | 0.5–40 ms | G | |
| chorus / Ch | rate_hz | Rate | 0.01–10 Hz | G | |
| chorus / Ch | rate_spread | Spread | 0–1 | L | |
| chorus / Ch | stereo | Stereo | 0–1 | L | |
| phaser / Ph | center_hz | Center | 50–12000 Hz | G | |
| phaser / Ph | depth | Depth | 0–1 | L | |
| phaser / Ph | feedback | Feedback | 0–0.95 | L | |
| phaser / Ph | stereo | Stereo | 0–1 | L | |
| phaser / Ph | rate_hz | Rate | 0.01–20 Hz | G | |
| flanger / Fl | time_ms | Time | 0.05–85 ms | G | |
| flanger / Fl | time_depth | Depth | 0–1 | L | |
| flanger / Fl | feedback | Feedback | 0–0.95 | L | |
| flanger / Fl | pan_depth | Pan Depth | 0–1 | L | M |
| flanger / Fl | stereo | Stereo | 0–1 | L | |
| flanger / Fl | rate_hz | Rate | 0.01–20 Hz | G | |
| filter / Flt | cutoff_hz | Cutoff | 20–20480 Hz | G | |
| filter / Flt | resonance | Resonance | 0–1 | L | |
| filter / Flt | saturation | Sat | 0–1 | L | |
| filter / Flt | mod_rate_hz | Mod Rate | 0.01–20 Hz | G | |
| filter / Flt | mod_depth | Mod Depth | 0–1 | L | |
| filter / Flt | level_db | Level | −60–24 dB | L | M |
| compressor / Comp | ratio | Ratio | 1–40 :1 | G | |
| compressor / Comp | threshold_db | Thresh | −60–0 dB | L | |
| compressor / Comp | attack_ms | Attack | 0.05–1000 ms | G | |
| compressor / Comp | release_ms | Release | 1–5000 ms | G | |
| compressor / Comp | out_db | Output | −60–24 dB | L | M |
| crusher / Cr | bits | Bits | 1–16.01 bit | L | |
| crusher / Cr | divider | Divider | 1–32 | G | |
| crusher / Cr | cutoff_hz | Cutoff | 20–20480 Hz | G | |
| crusher / Cr | resonance | Resonance | 0–1 | L | |
| gator / Gt | smooth | Smooth | 0–1 | L | |
| eq / EQ | freq1_hz | Freq 1 | 20–20480 Hz | G | |
| eq / EQ | gain1_db | Gain 1 | −24–24 dB | L | |
| eq / EQ | q1 | Q 1 | 0.08–12.5 | G | |
| eq / EQ | freq2_hz | Freq 2 | 20–20480 Hz | G | |
| eq / EQ | gain2_db | Gain 2 | −24–24 dB | L | |
| eq / EQ | q2 | Q 2 | 0.08–12.5 | G | |
| eq / EQ | low_hz | Low Freq | 20–1000 Hz | G | |
| eq / EQ | low_db | Low Gain | −24–24 dB | L | |
| eq / EQ | high_hz | High Freq | 440–11000 Hz | G | |
| eq / EQ | high_db | High Gain | −24–24 dB | L | |
| wah / Wah | cutoff_hz | Cutoff | 20–20480 Hz | G | |
| wah / Wah | resonance | Resonance | 0–1 | L | |
| wah / Wah | sensitivity | Sens | −1–1 | L | |
| wah / Wah | attack_s | Attack | 0.001–8 s | G | |
| wah / Wah | release_s | Release | 0.001–8 s | G | |
| wah / Wah | level | Level | 0–10 | L | M |

Required DSP work and qualifications:

- `Effect` currently exposes wet/tempo/song position, but no typed live setters
  for these controls. Most coefficients are assigned in `prepare`, which can
  allocate or clear state. Add fixed typed snapshots and control-rate setters;
  preserve delay history, modulation phase, filter states and compressor detector.
  Do not call `prepare` or mutate parameter maps for each automation tick.
- Reverb predelay currently has roughly 300 ms capacity, delay buffers roughly
  4 seconds (mod delay 4.1 seconds), despite larger UI ranges. Reverb comb capacity
  also depends on its initial modulation depth. Allocate the maximum supported
  capacity off the audio thread before publishing those ranges. Define delay-time
  slew/crossfade behavior; an abrupt read-head jump is not acceptable modulation.
- Preserve filter stability during coefficient changes. Crusher bypass changes
  around 20 kHz currently need a smooth transition. Fractional crusher bits and
  divider values are supported by the DSP: do not turn them into integer enums.
- Compressor ratio/threshold also change automatic makeup gain; retain detector
  history and smooth gain. FX saturation also changes level. Use conservative
  Opti excursions; full API range is not a recommended LFO excursion.
- EQ Q is clamped to 0.08–12.5 in DSP; the editor's 0.1–20 differs. Compressor
  output supports −60 dB in DSP versus −24 dB in the editor. Ping-pong missing-field
  delay defaults are 250/250 ms in DSP versus 300/450 in the editor. Preserve DSP
  behavior and correct the editor during implementation, not by changing old state.
- Gator `step_beats` changes song-position phase/pattern interpretation. Keep it
  out of this continuous proposal, with pattern/link, sync, stage counts, filter
  types, polarity and other selectors. EQ gains are tone-shaping candidates;
  overall output gains and direct pan controls are manual-only.

## State recall must be fixed before appending parameters

Current `setStateInformation` applies **all** host live values whenever
`liveParameterSchema >= 1`. Appending controls naively would overwrite fields in
older embedded Patches with missing-parameter defaults or stale instance values.

Propose `liveParameterSchema=2` (distinct from exporter report schema 3):

1. Collect actually present parameter IDs before APVTS replacement. Restore all
   existing saved controls without renormalizing them unnecessarily.
2. Derive absent new controls from embedded `Patch` and existing DSP defaults;
   overlay only parameter values present in the saved state. Publish the completed
   snapshot under the existing load guard before enqueuing the engine.
3. Initialize inactive typed-FX banks deterministically on every old-state/preset
   load. Save those banks in new states; only the active type writes into the
   Patch FX map. A type change must select/initialize that type's values without
   allowing stale controls from the previous type to overwrite its map.
4. Preserve `LoadedPatch`, metadata, edited flag, unknown fields and saved trim.
   Recalling a state must not remeasure trim. Retain old compatibility fallbacks.
5. Replace hardcoded 60/62 field maps/counts in editor/exporter/verification with
   the same authoritative registry used for APVTS and Patch conversion. New host
   floats follow the qualified bounded comparison; non-host fields remain exact.

## Implementation order and Opti handoff

1. Implement registry and presence-aware migration, then matrix amounts and the
   envelope controls with their required active-voice semantics. Keep unsupported
   entries out of the advertised API until functional; append shipped additions
   without later inserting into old positions.
2. Add typed FX setters and qualify scalar/coefficient controls; then finish the
   buffer-dependent delay/reverb controls. Publish each shipped subset explicitly.
3. For each actual build, export IDs/names/ranges/activity conditions. Opti owns
   profile changes and configured Rack subsets. Existing 62-control Racks must
   still load with unchanged state and mappings. New controls require an updated
   configuration; adding VST3 parameters does not itself configure old Racks.
4. Qualification is targeted: old-state migration and repeated preset loads,
   active/new voice behavior, FX history/finite output, no allocation/rebuild on
   automation, hosted ID compatibility and native recall. Reuse existing exporter
   regressions; do not rerun all 2,648 exports to assess this proposal. A corpus
   migration/export is a later packaging decision after the new API is qualified.
5. Actual Live discovery, audible modulation and Rack reopen remain user checks.
   No Live UI actions are authorized by this handoff.

Ableton documents automatic population for up to 64 parameters, and Configure for
larger plug-ins. Configure state is separate from preset state and can be saved
in Racks/Sets. Verify usable host capacity and choose preset-relevant subsets;
this proposal does not claim that all 620 can be exposed in a single Live device.
Sources: [saving configurations](https://help.ableton.com/hc/en-us/articles/209073089-Saving-plug-in-parameter-configurations),
[Live 12 plug-in manual](https://www.ableton.com/en/live-manual/12/using-plug-ins/).

Local evidence: `src/core/live_controls.h`, `patch.h`, `envelope.h/.cpp`,
`engine.cpp` (`resolveMatrix`, voice start/modulators), `effects.h/.cpp`,
`plugin/Source/PluginEditor.cpp` (envelope/FX editor),
`plugin/Source/PluginProcessor.cpp` (state save/restore), and
`plugin/Tests/PresetExport.h` (host/non-host state comparison).
