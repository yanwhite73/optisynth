# Native VST3 preset export

10 November 2026: the Opti owner requested an offline native exporter for the
existing Rack builder. First qualify `Pad/pads gated/jam Spectral Beauty.ptch`;
preserve the staged corpus's relative folders when batch export is later approved.
No Live operations, plugin installation or DSP changes are part of this work.

**Latest user result, relayed by the Opti owner:** the sample Rack loads in Live,
but Yan says it sounds quite wrong and explicitly separates this plugin-fidelity
issue from the preset export path. Rack loading is accepted; sound fidelity and
saved-Rack reopen are not accepted by that statement. Opti owns the authorized
2,648-entry batch and installation in its own `Yan White/Optispectral` folder.

**Requested bulk mode:** add explicit `--state-only` to this helper. Keep normal
patch loading/auto-level, actual SDK export and fresh native recall, exact state,
62 controls and bypass checks; omit audio comparison/audibility requirements and
label the report accordingly. Default full-audio behavior stays. Qualify the new
sample against the existing Comp/Cont bytes and prove a silent fixture can export
only with the explicit state-only flag. No production or installed plugin changes.

## Mechanism and acceptance criteria

The helper lives in `plugin/Tests/PartialsStress`. It loads a `.ptch` through
`PartialsProcessor::loadPresetFile`, waits for loading to finish and rejects errors.
At 48 kHz this calculates the same 1.5-second automatic level measurement as the
app. `getStateInformation` then saves all APVTS values, embedded current/original
patches and measured trim. `applyEditedPatch` is unsuitable because it skips that
measurement.

The JUCE 8.0.8 host's source-defined `VST3PluginState` envelope carries these bytes
as its `IComponent` child using `MemoryBlock::toBase64Encoding`. Host
`setStateInformation` supplies them to the real component, then synchronizes the
controller and parameter cache. The plugin wrapper accepts processor state
without its optional private bypass footer. A fresh instance supplies bypass-off.
This is a pinned JUCE bridge, not a portable file format or a hand-built VST3 file.

`ExtensionsVisitor::VST3Client::getPreset` calls Steinberg `PresetFile::savePreset`
with the actual component/controller. A fresh instance reloads the resulting file
with `setPreset` / `PresetFile::loadPreset`. Visitor references must not escape
their callback. Native component state, all 62 processor parameter values and
audio must survive recall; export fails closed otherwise. The helper must not
silently overwrite an existing destination or report native Live acceptance.

Evidence read: JUCE `juce_VST3PluginFormat.cpp` methods `getStateInformation`,
`setStateInformation`, `appendStateFrom`, `createMemoryStreamForState`,
`getStateForPresetFile`, `setStateFromPresetFile`; `juce_ExtensionsVisitor.h`;
`juce_audio_plugin_client_VST3.cpp` state methods; bundled SDK
`public.sdk/source/vst/vstpresetfile.cpp` save/load and component/controller restore.
The already-reviewed JUCE/SDK dependency is unchanged. Only our exact local bundle
is loaded. No scanning, network, audio devices or MIDI ports are used. Writes are
the requested preset and its qualification report. Remove those files and the
helper build to undo. A bridge/version mismatch risks incorrect Rack sound/state;
fresh-instance state and audio checks are required before handing over a sample.

## Qualification

Implemented in `plugin/Tests/PresetExport.h`, exposed by `PartialsStress`:

```sh
plugin/Tests/Builds/PartialsStressMacOSX/build/Release/PartialsStress \
  --export-vstpreset [--state-only] INPUT.ptch OUTPUT.vstpreset [BUNDLE.vst3]
```

Run from `partials/` to use the default local Release bundle, or supply an absolute
bundle path. Use the shared workload launcher with `--kind test`. Output must be
new; its sibling `.vstpreset.json` records qualification. Each invocation handles
one preset, so a future batch caller must preserve source-relative folders.

**Sample passed:**
`build/native_preset_export/Pad/pads gated/jam Spectral Beauty.vstpreset` (81,886 bytes).
SHA-256: `a8ba3c05ef85c62f493fe2f36973f734014409578973951efa61aaeabf02e2ae`.
The native preset writer produced component CID
`ABCDEF019182FAEB597768745072746C`; fresh native preset recall preserved the entire
embedded component XML exactly, all 62 host values and bypass-off. The measured
trim is **+5.751705169677734 dB**. At 48 kHz / 256 frames, 128,000 stereo frames
(C3 note-on, note-off and release at 120 BPM) matched fresh direct processor recall
**sample-exactly**, max absolute and relative RMS error both zero; peak 0.2511888444.
The on-disk bytes match the SDK-produced preset. Release arm64 harness build passed.
Overwrite and missing-input refusal checks passed; the accepted preset and report
hashes stayed unchanged (`build/native_preset_export/negative_checks.json`).

The reference is a fresh state recall: the initial loader consumes random numbers
during trim measurement, so comparing its already-used engine would test different
random initial conditions. After native `setPreset`, JUCE's cached host parameter
objects are synchronized by a host-state roundtrip; the exact component state is
checked both before and after this step. VST3 exposes no loader-ready flag. A
one-second grace interval precedes the render, and a complete audio match, rather
than elapsed time, determines acceptance. Silent presets fail this audio check;
this single audible sample does not yet qualify a full-corpus batch policy.

**Opt-in state-only mode qualified:** `--state-only` preserves the normal source
load/auto-level measurement and all native export, fresh recall, exact component
state, 62 parameter and bypass checks. It skips the subsequent reference/audio
render and audibility test. Reports retain schema 1 with additive fields:
`verificationMode: "state-only"`, `audioVerified: false`, `audioSkippedReason`,
`sourceAutoLevelMeasured: true`, and `bypassVerified: true`; audio metrics are
absent. `passed: true` means state acceptance in this mode, not audio acceptance
or evidence that the asynchronous VST3 engine is ready to play. Without the flag,
the mode is `state-and-audio`, `audioVerified` is true on success and all existing
audio thresholds remain. Unknown or duplicate flags are rejected.

`tools/qualify_native_export.py` passed seven cases: both modes on the original
sample, silent state-only export, default silent refusal, duplicate/unknown flag
refusal, and overwrite refusal. The entire native file from both audible-sample
modes matches the previous 81,886-byte sample exactly, so Comp/Cont and CID are
unchanged. The silent fixture retained zero measured trim and all 62 controls;
no default-audio output was published. Evidence:
`build/native_preset_export/state_only_qualification/qualification.json`.
Qualified helper SHA-256:
`13413a1caf03ab1e3079b252eaa108380cb096624621813e1d50387af42dddae`.

The local and installed 0.1.1 VST3 executable hashes remain
`e1142c3907d0bca1f7876350574088bff0baa3d61177e29f339b510b84e5f2cc`.
No DSP or installed bundle changed. Opti received the updated helper and owns the
authorized full batch and installation. Yan accepted the sample Rack loading in
Live; its sound mismatch remains open in `design.md`. The offline audio comparison
is with our direct processor, not the original Spectral, and does not settle that
sound complaint. Saved-Rack reopen was not confirmed by the load report.

**First bulk exception:** Opti reports 19/20 successful and an exact-state failure
for `Uncategorised/Arksun BigTone/BAS staccato bass.ptch`, before output. Batch
paused, valid outputs preserved. Add stage-specific expected/actual diagnostic
XML without changing acceptance, reproduce in a separate output directory, then
inspect the exact difference before deciding whether any comparison correction
is justified. Do not change DSP or silently relax the generic check.

**Diagnosis:** the `processor-to-vst3` diagnostic differs only in current Patch
`osc.1.symmetry`: `0.550000131` -> `0.550000191`, one float ULP. The persisted
`PARAM osc1_symmetry` stays exactly `0.5500001311302185`; all other XML is exact.
JUCE's APVTS `setDenormalisedValue`, parameter `setValue/getValue`, and adapter
`parameterValueChanged` perform float normalization/denormalization. `flushToTree`
retains approximately equal persisted values, while `editablePatch()` overlays the
raw control value. This is a duplicated derived field, not a VST3 container error.

**Correction:** use an independent fresh direct-processor recall to derive the
exact expected current Patch. Require all persisted APVTS values, trim, LoadedPatch
and metadata to remain source-exact. Require every non-host Patch line to remain
source-exact; only the 60 known host-control lines may change, and the native VST3
must match the independent recalled state exactly. No numeric tolerance is added.
The SDK still writes the actual plugin state; no native bytes are manufactured or
rewritten. Reports now use schema 2: `embeddedStateExact` retains source-equality
meaning (false for BAS staccato), `recalledStateExact` is true on acceptance,
`persistedParametersAndNonHostFieldsExact` is true, and `stateComparison` is
`exact-direct-recall-with-source-field-guards`. Existing state-only/audio semantics
are unchanged. Phase-specific failure diagnostics remain available beside outputs.

**Qualified:** all eight cases pass, including BAS staccato bass, both previous
sample modes (native bytes still identical), silence and refusal checks. Evidence:
`build/native_preset_export/recall_qualification/qualification.json` and the
regression report beside it. Final helper SHA-256:
`34819a70e29c5581a2aeb4ba15a01896261a1a00d499a365ce01d68902cdd856`.
Local/installed plugin executable hashes remain unchanged. Opti owns the resumed
batch and its schema-2 acceptance logic; v1 pilot outputs remain preserved.

11 November: v2 stopped at 114 entries (112 pass) on `ld Sneaky Brass` and
`pad Retro Grunge`, both at the direct-recall persisted-state guard. Diagnose
both once with XML evidence, then qualify one common host-float policy using
the existing <=1e-6 normalized control contract. Keep non-host Patch,
LoadedPatch, metadata and trim exact; report bounded equivalence separately
from exact equality. Exporter-only scope; Opti owns batch/install.

Both diagnostic runs confirmed only `osc1_symmetry` PARAM and the matching current
Patch line changed: Sneaky Brass `0.2099999636411667` -> `0.2099999338388443`;
Retro Grunge `0.3400000631809235` -> `0.3400001227855682`.
Evidence: `build/native_preset_export/precision_diagnostics/differences.json`.

Common policy (schema 3): finite, in-range values for known host float PARAMs and
their 60 duplicated current-Patch fields may differ by at most `1e-6` after the
parameter's NormalisableRange conversion to 0..1. Use unsnapped range conversion
so stepped controls cannot hide a larger physical difference. Discrete controls,
non-host Patch lines, LoadedPatch, XML structure/IDs, metadata and trim remain
exact. Compare every native stage against both the original source and the direct
recall reference to bound total drift, not just each hop. Accepted differences
are recorded by stage, field, both values and normalized delta; failures dump XML.

Report acceptance: `schema:3`, `passed:true`,
`stateComparison:"bounded-host-floats-exact-non-host-state"`,
`hostParameterValuesWithinTolerance:true`,
`hostParameterNormalizedTolerance:1e-6`,
`maxHostParameterNormalizedDelta <= 1e-6`, `nonHostStateExact:true`, alongside
the existing 62 controls, native recall, bypass and auto-level checks.
`embeddedStateExact`, `recalledStateExact`, `persistedParametersExact` and legacy
`persistedParametersAndNonHostFieldsExact` report actual equality and may be false.
They are not substitutes for the bounded policy. Audio-mode semantics are unchanged.

**Qualified 11 November:** 11 integration cases and 1,227 comparator checks pass.
The latter exercise every host float and every current-Patch mapping at five
positions across linear/log ranges, inside/outside the bound, plus invalid values,
discrete controls, identifiers, structure and protected-state changes. All three
regressions now export: maximum normalized deltas are BAS staccato
`1.192092895507812e-7`, Sneaky Brass `5.960464477539062e-8`, Retro Grunge
`1.192092895507812e-7`. Exactness flags are false where appropriate. The original
sample's native bytes remain unchanged in both modes.
Evidence: `build/native_preset_export/precision_qualification/qualification.json`
and `common-float-policy.log`. Qualified helper SHA-256:
`964be92894e0e5e4ebef2e65ff18727b584dc68e4a6cc13caf79df3434279df4`.
Opti confirmed pinning it for its v3 batch. DSP and installed plugin remain unchanged.
