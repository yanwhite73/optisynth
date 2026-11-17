# Optispectral parameter handoff to Optimystery — 10 November 2026

**11 November: 0.1.2 installed.** See
[expanded control delivery](host_parameter_extension.md). The plugin exposes 620
controls with old-state migration and live envelope/matrix/FX updates. Exported
registries are authoritative; ranges were widened after auditing real presets.
Original IDs remain unchanged. Opti's owner is integrating configured subsets;
its current 39-target profile remains the last verified active profile until that
update is installed. Verification and performance limits are in the delivery doc.

**Product rename:** Yan named the synth **Optispectral** before its first VST3
installation. Public product/app/bundle name changes in 0.1.1. The `partials/`
source folder, preset/state format, parameter IDs/names and plugin codes remain
unchanged. Opti should accept Optispectral as well as legacy partials/Partials,
retaining its existing profile identity for portable recall. Earlier Partials
qualification and owner reports below are historical; native Live verification
remains outstanding. Yan authorized installing the renamed VST3 only; that copy
is now installed and verified at `~/Library/Audio/Plug-Ins/VST3/Optispectral.vst3`.
The actual 0.1.1 bundle passes Release D/E/V. All 62 parameter entries, 63 VST3
automation IDs and three class CIDs match the old name's build. Its executable
SHA-256 is `e1142c3907d0bca1f7876350574088bff0baa3d61177e29f339b510b84e5f2cc`.
Strict signature verification and all six bundle-file hashes match the qualified
source. Receipt: `../build/name_qualification/install_receipt.json`. Opti's owner
confirmed **dev.52 installed at 18:32:08** with exact case-insensitive Optispectral
and legacy Partials aliases, the unchanged `partials-v1` profile and 39 targets.
Real dev.51-exported descriptors restore under the new name; both rename directions,
title changes and configured-order changes pass offline. The qualification record
reports 60 Python checks, three source/archive Node suites and 10 plugin groups
per run, with 17 installed files matching the ZIP and 19 preserved. See
`../../ableton/randomizer/generator_presets_v2/qualification/dev52-optispectral-name/README.md`.
A fresh main Optimystery dev.52 activates this update; keep existing opticapture,
and no additional restart is needed for this alias change. Native discovery,
audible movement and Rack/Set reopen remain unverified user checks.

Yan wants Optimystery's LFOs to access Partials, and explicitly asked this session to
pass that request to the Opti session. This is a compatibility task, not authorization
to drive Live. Partials source work belongs to Codex thread
`01a1bff2-d7d6-7441-ac66-3c732e4b3093`; coordinate rather than edit that source concurrently.

**Native preset handoff (10 November):** the offline exporter now supplies an actual
SDK-written `.vstpreset` for `Pad/pads gated/jam Spectral Beauty`, with exact component
state, 62 controls, trim and sample-exact audio recall through a fresh VST3 instance.
The sample is `../build/native_preset_export/Pad/pads gated/jam Spectral Beauty.vstpreset`.
See [native preset export](native_preset_export.md) for the command, report, hash and
limits. Opti owns its Rack builder and category-preserving 2,648-file source staging.
Yan subsequently confirmed the Rack loads in Live, but sounds quite wrong; he
explicitly separates this plugin fidelity issue from the preset path. Opti owns
the authorized batch/install in `Yan White/Optispectral`. The helper's explicit
`--state-only` mode is now qualified: all normal auto-level/state/62 controls/bypass
checks remain, audio acceptance is skipped and clearly labelled. Seven checks
pass, including byte-identical Comp/Cont against the sample and silent export with
the default audio refusal unchanged. See the export document for the CLI hash.
Full-corpus results belong to Opti's batch report; sound fidelity and saved-Rack
reopen remain unconfirmed by the successful-load report.

The 10 November build adds 60 continuous parameters alongside legacy Output and Auto
level: **62 processor parameters**. Their audio-path updates work with no editor and
do not enqueue rebuilds. See `host_parameters.json` for authoritative IDs, host names,
units, ranges, normalization, step intervals, version hints and LFO eligibility.
The four oscillator/filter pairs, five LFO rates, main volume and six wet controls
are covered. IDs/order are append-only; legacy parameters keep version hint 1 and
the additions use 2. Names are unique and at most 16 characters. Volume, pan, Output,
Auto level and currently incomplete Filter XMod/RM are excluded from automatic LFO
selection. Other candidates have explicit conditions (enabled source/filter/effect,
free-running rather than synced LFO). Candidate status does not prove audible effect
in every preset.

The actual local VST3 was loaded in the offline host test. `vst3_parameters.json`
records **63 automatable parameters (62 plus wrapper bypass)** and **2,080 MIDI
proxy parameters that are not automatable**. Do not offer these proxies/bypass as LFO
targets. Live's actual visible/configured list still needs user verification; the raw
VST3 parameter count alone does not establish automatic Configure behavior. Permanent
JUCE string IDs, numeric VST3 IDs, Live parameter indices and runtime DeviceParameter
IDs are distinct. In Live, match the verified unique names and validate recall.

Local bundle: `plugin/Builds/PartialsMacOSX/build/Release/Optispectral.vst3`.
AU/Standalone remain local builds. Only the VST3 is installed; it is untested in Live. Offline tests
cover real bundle load, MIDI synthesis, parameter-driven audio changes and container
state recall, plus direct processor sanitizer tests. Smoothing is approximately 4 ms
at control-block boundaries; JUCE 8.0.8 takes the last host automation point per block.
Do not promise sample-accurate or audio-rate external modulation.

At handoff the Optimystery source `generator_presets_v2/src/modulation.js:359` explicitly
skips device classes matching `^(Mx|Plugin)`. This blocks plugin candidates before their
parameters can be examined. `docs/REQUEST_QUEUE.md` Q6 already tracks broader LFO targets.
Please record this Partials-specific request there and plan compatible plugin discovery,
identity/recall and continuous-target selection using the existing working modulation
lifecycle. It is not sufficient just to delete the class filter: native catalogue matching
and candidate eligibility must be checked. Keep quantized/stateful switches separate from
continuous musical targets; do not infer parameter index or map by GUI position.

Mechanism and primary sources:
- VST3 exports normalized host parameters with permanent IDs and automation flags:
  https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Parameters%2BAutomation/Index.html
- Live exposes configured VST/AU controls; <=64 parameters populate automatically, larger
  sets need Configure. Defaults can be saved independently of preset state:
  https://help.ableton.com/hc/en-us/articles/209073089-Saving-plug-in-parameter-configurations
- `live.remote~` takes a DeviceParameter ID and continuous signal, controlling it without
  writing the saved base value or creating undo steps; releasing id 0 restores control:
  https://docs.cycling74.com/reference/live.remote~/

Success evidence must distinguish Partials' offline host-parameter/DSP tests from native
Live discovery, live.remote binding/movement and saved-Rack recall. Native checks remain
user-performed unless separately authorized. Please report findings back to this thread.

## Optimystery owner report — dev.51, 10 November 2026

The Opti session reports dev.51 implemented, qualified and installed at 12:24:49,
with the exact 39 agreed candidates and product/unique-name discovery. Portable
recall tolerates title and configured-order changes within the same device path
and ancestry; missing or duplicate controls remain lost. The owner reports source
and packaged-runtime LFO/portable/Partials suites and 76 Python checks passing,
with the existing DSP graph and 375-parameter contract unchanged. This is the
Opti owner's installation/qualification report, not independent Live verification
by this Partials session. Its mechanism record is
`../../ableton/randomizer/generator_presets_v2/qualification/dev51-partials-lfo/README.md`.

Native acceptance remains outstanding: actual product/parameter discovery and
Configure behavior, audible/visible movement with the Partials editor closed,
Off/bypass recovery, and saved-Rack reopen/recall. A fresh main Optimystery dev.51
device was required for that runtime. At the time of this owner report, the Partials
plugin was not installed or loaded into Live. The subsequent renamed VST3 installation
is recorded above; actual Live checks remain outstanding.
