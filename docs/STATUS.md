# Optisynth status — 17 November 2026

## Current delivery: contextual hardware rack, standalone 0.0.4

The app now exposes **17 module types** in a rack with multiple independent
instances, contextual controls, typed input connections and output selection.
Original Plaits VA/sine oscillators, modal percussion, bass drum and an externally
excited resonator are available, together with original stmlib digital LP/BP/HP
filters. These filters are the digital library algorithm, not a Ripples circuit
emulation. Spectral ADSR/AHDSFR, Spectral LP/HP/BP/notch, bypass, VCA and mixer remain.

Four starter patches: VA + digital filter, modal percussion, drum into resonator,
and two differently tuned oscillators sharing a filter/envelope. Mixer inputs have
independent level controls. All loaded panels appear side by side, with blue
oscillators, green filters, dark envelopes/utilities and purple resonators. Module
type determines the knob/fader arrangement. Click sockets to patch; hover to see
connections. Focus module opens the enlarged panel and connection list.
Only reachable modules are constructed; dormant nodes keep editable parameter state.

App: `../app/Builds/MacOSX/build/Release/Optisynth.app`, Apple Silicon,
version 0.0.4, bundle `com.yanwhite.optisynth.app`. Click Start audio, then play the
piano or Caps Lock typing keyboard (A/W/S/E/D/F..., Z/X octave). Silence stops the
output device. Pitch/octave labels are consistent across the piano and typing help.
No microphone, hardware MIDI or network listener is opened by this app.

Full `.optisynth` rack JSON preserves node IDs, separate instance parameters,
connections, outputs and gain. Old 0.0.2 instrument patches import with the same
active sound. Invalid connections, cycles, controls and malformed files are rejected
before replacing the graph. Required inputs may be incomplete only on dormant nodes;
reroute downstream destinations/output before removing an active dependency.

## Verification

- Native 0.0.4 release build succeeds. At 0.0.3, 1,194 inherited core checks, 28 graph checks,
  74 modular checks, four importer checks and one renderer timing check pass.
- All 74 modular checks also passed under AddressSanitizer/UBSan at 0.0.3. Coverage:
  oscillator/filter/resonator extremes, no render allocation, independent same-type
  instances, dormant nodes, audio-only roots, filter response and hard sync.
- Existing sample-rate conversion checks cover 44.1/48/96/192 kHz, finite bounded
  output, plan crossfades and no allocation in the tested core render paths.
- Native UI checks: independent knob bindings, graph JSON and byte-identical audio
  recall, failed-edit preservation, dormant add/remove, typed disconnect/reconnect,
  and byte-identical old-format import. At 0.0.4 these checks were rerun, with new
  rack/focus synchronization, socket action/indicator/tooltip, contextual replacement
  and all-17-panel bounds/overlap checks. No audio device was opened for these checks.
- Six native captures cover rack/focus, resonator, two oscillators/mixer and two
  1120x800 layouts. Cramped controls were corrected and the final visual review passes:
  `../design-qa.md`. Receipt: `hardware_ui_build_receipt.json`. The earlier DSP
  receipt remains `rack_build_receipt.json`; its sound-engine sources are unchanged.
- DSP qualification: `../build/modular-qualification.log`; native receipt and PNGs:
  `../build/ui-review/`. Importer ResourceWarnings are inherited file-handle warnings.
- Core benchmark remains the existing two-source case: 104.17 s of audio in
  0.506 s (~0.49% of one core), excluding device converter/UI. This is regression
  evidence, not a worst-case rack deadline or a new optimisation claim.

Yan auditioned 0.0.2 and reported nice tones, then confirmed the Mutable examples
in 0.0.3 all seem to work (request 25). Yan approved the 0.0.4 UI direction with
“thats good” (request 26); this is visual feedback, not a separate audio test. No Live verification or new VST3
packaging/installation is claimed.

## Current direction and exact next work

Requests 17–19 (keep building; show available open-source Mutable modules) are
implemented in this rack milestone. Request 20's other-maker shortlist is recorded
in `OPEN_SOURCE_MODULE_MAKERS.md`, indexed by `RESEARCH.md`: 4ms Ensemble Oscillator
and DaisySP are the recommended next sources to review. No code from those projects
has been adopted. All 26 user requests are retained verbatim in `USER_REQUESTS.md`.

Request 25 is implemented: Matriarch-inspired native rack with familiar,
module-specific panels. References and behaviour are in `CONTEXTUAL_UI.md`.
DSP/patch format remain unchanged; sample playback is next.

Requests 23–24: continue implementation across side-note requests. The current
Mutable rack was opened and auditioned successfully; this standing continuity
instruction remains in force after the UI milestone.

Request 22 adds a parallel Korg Wavestate repurposing project: a potential physical
controller or eventual hardware host for Optisynth. Both roles are recorded in
`RESEARCH.md`; integration and hosting feasibility remain unverified. The current
module/sample/polyphony development order continues.

Next action: define immutable sample assets and loader/patch persistence, then
implement a sample player that can feed the resonator or mix an attack alongside it,
then bounded polyphony and an explicit voice/global boundary for shared effects.
Further Mutable engines, granular playback and additional envelope/modulator
options follow. Preserve the current typed graph and stable parameter identities.
Hardware MIDI, audio-rate parameter modulation, event cables, drag-cable interaction,
agent API and a modular VST3 wrapper are still pending. This is a monophonic rack;
all modules receive the same note context. Hardware audio input is not implemented.

Source adapters currently target fixed 48 kHz / 24-sample blocks. The bounded output
converter handles other device rates. Structural edits prepare on the UI thread
and crossfade between plans; scalar edits use resolved atomics. Expensive future
sample preparation should move to a dedicated loader before adding large assets.

## Deferred sound-quality issue

Yan reported some perceived aliasing with both envelope routes enabled in 0.0.2,
then explicitly asked to log it and defer performance/sound-quality refinement.
Exact patch/settings are unknown. Source inspection found block-rate cutoff updates;
stepping, saturation and clipping are candidates, not confirmed diagnoses. No tuning
change was made for that report. On resumption, preserve a before render and compare
all four route combinations before choosing a fix.

## Foundation and boundaries

This separate project was copied from Optispectral (`../partials`), whose checkout
had no commits. The 83 copied source/doc/helper hashes are recorded in
`source_snapshot_20261117.json`. The inherited compatibility engine and its plugin
source remain separate from the new modular runtime. The Spectral table-reading
extraction previously produced byte-identical Filth Bass, fn HiHat and Tortured Lead
2 comparisons. Original software/demo-noise boundaries remain unchanged.

Graph compilation rejects cycles and malformed/oversized structures, prunes dormant
nodes and prepares only active dependencies. Maximum 128 nodes, 1,024 connections,
64 ports/outputs. Construction/destruction are off the audio callback; scalar edits
and the tested render paths allocate nothing. JUCE/OS callback synchronization is
separate from these core guarantees.

Original Mutable DSP and dependencies are pinned with notices and exact source hashes
in `third_party/mutable/PROVENANCE.json`; reviews are in `MUTABLE_FIRST_SLICE.md` and
`RACK_ROUTING.md`. Five classic-synth manuals/schematic archives (~38 MB) are stored
under ignored `docs/references/`; detailed Xpander/JP-8080 analysis remains pending.

## Backups and Git

Yan explicitly authorized regular commits and pushes on 17 November (request 21),
superseding the inherited no-commit rule for this project. Origin is
https://github.com/yanwhite73/optisynth.git. Push checked milestones and verify remote
branch equality. No changes or commits to the parent Optispectral project.

Independent source archives remain enabled via `tools/backup_source.py`. They now
include all three app headers and our `.optisynth` patch files. Previous verified
archives: `../../backups/optisynth/optisynth-source-20261117-163238.tar.gz` (136 files)
and `optisynth-source-20261117-155932.tar.gz` (98 files). Each has a checksum and an
internal file manifest. The current delivery's archive and Git receipt are recorded
in `ENGINEERING_LOG.md` after verification.
