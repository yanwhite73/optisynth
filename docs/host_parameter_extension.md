# Optispectral 0.1.2: expanded host controls

Implementation: 620 processor parameters (existing 62 plus 558 additions).
Qualified and installed on 11 November 2026; results are recorded below.
Authoritative contracts: host_parameters.json (IDs, names, ranges, normalization
and conditions) and vst3_parameters.json (actual numeric VST3 IDs).
The proposal is retained as design history.

## What changed

- Nine controls for each of 12 envelopes: attack, hold, decay, sustain, fade,
  release and attack/decay/release curves.
- All 24 modulation-matrix amounts. Route source/destination remain structural.
- 71 detailed controls for each of six FX slots, with stable type-specific IDs.
  All 13 effects implement continuous updates. Existing wet controls are retained.
- Editor controls use the same host parameters. Fade displays dB/s; the unused
  fade-curve knob is disabled and labelled as unimplemented.

All original 62 controls keep their IDs, names, order, ranges and version hints.
New controls have version hint 3; plugin codes, bundle identity and class IDs
remain unchanged. New controls use the IDs and short names in the proposal.

## Real-time behavior

The engine smooths physical values at its 16-sample control interval. Updates use
fixed arrays and typed setters, without rebuilding tables, reallocating effect
buffers, clearing tails or retriggering voices. Delay-time changes also slew per
sample. Internal modulation phase and detector/filter histories are retained.
Host automation retains the wrapper's block-rate behavior; this does not promise
sample-accurate external modulation.

Envelope edits preserve stage progress and current level, rebasing the remaining
curve. Held sustain responds smoothly; changing attack does not restart a
sustained note, and completed voices are not revived. High envelope curves use a
finite calculation. Matrix amounts are read through original row indices by
current and new voices, retaining matrix-depth routing semantics.

EQ keeps its static RBJ transfer functions but uses trapezoidal integrator state
when coefficients move. The former direct-form state became unstable under
extreme sweeps. Independent bounds protect against invalid/hostile input.
Crusher filter bypass transitions are smoothed while filter history continues.

Reverb predelay now has capacity for its full two-second control range. Stereo
and ping-pong delay lines support eight seconds; mod delay reserves room for its
stereo offset and modulation. This fixes earlier clipping of long times:
presets relying on that clipping may have longer audible delays now.

## Range corrections from existing presets

The 2,636-file local imported bank contained values outside several proposed UI
ranges. The shipped ranges retain them:

| Control | Shipped physical range |
| --- | --- |
| Chorus time | 0.01–40 ms |
| Chorus rate | 0.001–10 Hz |
| Stereo/mod delay high-pass | 8–20480 Hz |
| Stereo/mod delay low-pass | 20–22000 Hz |
| Mod delay time | 0.01–8000 ms |
| Compressor attack | 0–1000 ms |
| Filter FX modulation rate | 0.01–320 Hz |
| Flanger rate | 0.001–20 Hz |
| Flanger feedback | 0–0.999 |
| Flanger time | 0.01–85 ms |

Existing DSP clamps remain: chorus time >=0.5 ms/rate >=0.01 Hz, compressor attack
>=0.05 ms, flanger time >=0.05 ms and feedback <=0.95. This preserves raw saved
values and their previous audible behavior. Clamped regions are not useful LFO
excursions. Other bounds follow the proposal and exported registry.

## Recall and integration

Saved state now uses liveParameterSchema=2. Loading older state derives missing
parameters from its embedded Patch and effect defaults, rather than Init values
or the previous preset. Present saved parameters take precedence. LoadedPatch,
unknown fields, edited state and saved trim are retained. New saves retain
inactive typed-FX parameter banks. The editor's explicit FX type selection
initializes that type to defaults; scalar edits preserve the other banks.

Preset comparison uses the shared registry for active Patch fields and parameter
counts. Bounded host-float comparison and exact non-host checks remain. Exporter
report schema 3 is separate from processor state schema 2.

Opti must keep the current 39-target profile until its update is qualified.
These are conditional targets, not 558 universally active LFO destinations.
Detailed FX controls require the matching enabled/routed type. Sync overrides
free-running time/rate controls. Envelope targets require an active consumer.
Matrix rows are explicit targets until routing and destination eligibility can
be determined. Overall gain and direct pan remain excluded from automatic choice.

Existing Racks still configure their original 62 controls. The Opti owner should
select relevant new controls from each embedded preset, preserve old bindings,
and verify Live's effective configuration capacity. The plugin migrates old
component state; this update does not itself require a corpus re-export.
Live discovery, audible modulation through Opti and saved-Rack reopen remain
user checks; offline qualification does not replace them.

No AU installation, Live UI operation, original demo modification, commit or upload
is part of this update. Original-Spectral fidelity complaints remain separate.

## Verification record

- Core release and AddressSanitizer/UndefinedBehaviorSanitizer: 1,194 checks each,
  zero failures; importer: four checks passed.
- Checks include each detailed effect control's audible response, held-note
  envelope/matrix changes, finite extremes, EQ static gain and zero allocations
  during the tested real-time updates/process calls.
- Plugin A/B/D/E/F/V passed on the final build; lifecycle C passed earlier.
  Editor checks cover envelope bindings, matrix row 24 and detailed FX edits.
  All 62 original VST3 numeric IDs and three class IDs are unchanged.
- 8,127 state-comparator checks passed. Native export/fresh recall preserves all
  620 controls and gives sample-exact audio over 128,000 stereo frames.
  The earlier float-rounding regression also passes.
- Extreme stress exceeded real-time deadlines despite finite/bounded output.
  Live performance and actual Opti mapping remain separate checks.
- Installed VST3: ~/Library/Audio/Plug-Ins/VST3/Optispectral.vst3; all six files
  match the qualified build and strict signature verification passes.
  SHA-256: 2fb1a145996a66291fa371cdb8a07eca13a72c99c879accd8c5a2c799d97c145.
  Previous bundle: ../backups/partials/vst3-before-20261111-192843/Optispectral.vst3,
  outside plugin search paths. Receipts/logs: ../build/host_extension/.
- Opti's owner is integrating the registry and preset-relevant configured subsets,
  retaining the existing 62 controls. No full corpus re-export.

Activation evidence (11 November): Yan reiterated that testing should use the VST
in Ableton. The installed bundle is verified 0.1.2, but Live 12.4.25b1 Log.txt shows
new instances at 19:31:57–19:32:18 still loading Optispectral v0.1.1, after the
19:28:43 installation. Live retained the old loaded module. User should save and
restart Live to load the replacement; no agent restart or UI action was performed.

Yan clarified that saved-preset activity is sufficient, but automatic LFO targets
must affect the sound: Main/Amp/Cut/Mod envelopes and assigned internal LFOs.
Dev.54's selector filters new additions but retains all 62 old Configure entries;
its runtime profile still admits all 39 old candidates. This leaves unassigned or
synced internal LFO rates eligible. The selector also treats eligible zero-depth
matrix rows as active envelope consumers, and does not gate unused curve stages
or flat EQ-band frequency/Q. These are Opti selection gaps, not absent plugin
parameters. Reported to the Opti owner for request capture and correction; no
plugin rebuild or live-state telemetry change requested.

### Preset activity: engine evidence for the Opti selector

Read-only audit, 11 November. The clarified request supersedes keeping all 62
original Configure entries in regenerated library Racks: preserve plugin IDs and
state, but omit inactive controls from automatic selection. Personal chains and
existing mappings remain untouched. Yan reaffirmed: "I just want saved presets
to have usable destinations." Scope is selection from stored preset assignments
and settings; tracking later manual edits is not requested.

- `Voice::start` starts Amp and Cut envelopes only for enabled oscillators with
  tables. Referencing an envelope in the matrix cannot activate a disabled
  oscillator's envelope. Cut envelopes used as matrix sources do not require the
  corresponding filter to be on. All three Mod envelopes and Main start normally.
- `runModulation` applies matrix depth in one pass using the depth row's base
  amount, then skips depth destinations in the ordinary destination pass. Depth
  into another depth row has no effect. A valid zero-amount ordinary row is a
  useful amount target, but its source envelope/LFO is effective only when the
  row has nonzero amount or a direct active depth row drives it.
- Host LFO rate requires an assigned consumer and `sync_beats <= 0`. Matrix
  `lfoN.speed` multiplies the computed rate even when synced; do not apply the
  host-rate sync exclusion to that matrix destination. Both need an eventual
  effective destination. Noise wave 6 produces a fresh random value per control
  block, independent of its phase/rate, so exclude its rate/speed as useful targets.
- `Voice::render` writes FIL taps even with the filter bypassed. Cross-modulation
  source indices 4..7 therefore require the oscillator, not filter-on. OSC taps
  include Amp and precede oscillator volume; FIL taps also include oscillator
  volume. Neither includes Main or filter volume. An off source used for AM can
  still change gain through `1 - amount + amount * source`; FM/PM need a signal.
- `Envelope::advance`: attack/release curves require positive duration; decay
  curve also needs sustain below 1. Time controls at zero can become effective
  when changed. Hold/decay timing with sustain 1 still matters if it delays Fade
  or one-shot release. `ignore_note_off` does not remove release: with fade zero,
  Sustain automatically enters Release. With nonzero fade it enters Fade instead,
  and ignores note-off; release controls are then unreachable. Fade needs nonzero
  sustain. Fade curve is not implemented/exposed.
- `TableBank::build` normalises spectra before rendering. Equal A/B audible
  spectra and phases give an ineffective wave mix; ignore harmonic 0 and phases
  of silent harmonics when comparing. Equal raw arrays are a safe definite-no-op
  check; normalised equality catches additional equivalent waves.
- A flat normalised spectral curve, resonance at/below .01 and LP Offset >=60.5
  leave cutoff with no active filter process to move. Active resonance matrix
  modulation can change that. Resonance itself can activate its band-pass even
  from zero. Cut envelope depth can be activated by `filterN.envdepth`; an existing
  positive depth at maximum base cutoff (or negative at minimum) has no distance
  to move in the direct envelope calculation.
- FX need a reachable input, matching type, on and nonzero wet. Rate controls need
  their modulation depth and free-time mode; zero-gain EQ frequency/Q is inert;
  crusher resonance is inert with its low-pass bypass (`cutoff >= 20000`, type 0).

These are structural eligibility rules, not a guarantee of an immediately obvious
change on every note. Sources driven by MIDI controllers depend on performance;
envelope edits can affect later stages or future notes. No extra plugin build was
needed for this selector correction.

Delivered 11 November at 20:27:36: the Opti owner installed corrected Configure
metadata for all 2,648 library Racks and the existing Aqueous Flutters audition
Rack. Installation receipt verifies all Rack hashes and preservation of sound
state, 73 sidecars and personal saves. Nineteen focused checks and six actual
Rack configurations against installed dev.54 passed. Evidence is in
`../../ableton/randomizer/generator_presets_v2/qualification/optispectral-active-config-20261111/`.
Live logs now show plugin v0.1.2 loaded at 20:26:31 and 20:26:53, so the earlier
restart advice is resolved. Load a fresh corrected library preset to use the
new selection; no further restart or software update is needed. Actual Live
modulation listening and saved-Rack reopen remain user verification.
