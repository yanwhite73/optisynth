# Optisynth status — 17 November 2026

## Current delivery: Mutable rack, standalone 0.0.3

The app now exposes **17 module types** in a rack with multiple independent
instances, contextual controls, typed input connections and output selection.
Original Plaits VA/sine oscillators, modal percussion, bass drum and an externally
excited resonator are available, together with original stmlib digital LP/BP/HP
filters. These filters are the digital library algorithm, not a Ripples circuit
emulation. Spectral ADSR/AHDSFR, Spectral LP/HP/BP/notch, bypass, VCA and mixer remain.

Four starter patches: VA + digital filter, modal percussion, drum into resonator,
and two differently tuned oscillators sharing a filter/envelope. Mixer inputs have
independent level controls. Select a rack node to edit its controls and input sources.
Only reachable modules are constructed; dormant nodes keep editable parameter state.

App: `../app/Builds/MacOSX/build/Release/Optisynth.app`, Apple Silicon,
version 0.0.3, bundle `com.yanwhite.optisynth.app`. Click Start audio, then play the
piano or Caps Lock typing keyboard (A/W/S/E/D/F..., Z/X octave). Silence stops the
output device. Pitch/octave labels are consistent across the piano and typing help.
No microphone, hardware MIDI or network listener is opened by this app.

Full `.optisynth` rack JSON preserves node IDs, separate instance parameters,
connections, outputs and gain. Old 0.0.2 instrument patches import with the same
active sound. Invalid connections, cycles, controls and malformed files are rejected
before replacing the graph. Required inputs may be incomplete only on dormant nodes;
reroute downstream destinations/output before removing an active dependency.

## Verification

- Native release build succeeds. 1,194 inherited core checks, 28 graph checks,
  74 modular checks, four importer checks and one renderer timing check pass.
- All 74 modular checks also pass under AddressSanitizer/UBSan. New coverage:
  oscillator/filter/resonator extremes, no render allocation, independent same-type
  instances, dormant nodes, audio-only roots, filter response and hard sync.
- Existing sample-rate conversion checks cover 44.1/48/96/192 kHz, finite bounded
  output, plan crossfades and no allocation in the tested core render paths.
- Native UI checks: independent knob bindings, graph JSON and byte-identical audio
  recall, failed-edit preservation, dormant add/remove, typed disconnect/reconnect,
  and byte-identical old-format import. No audio device was opened for these checks.
- Captures cover oscillator, filter, external resonator and 1120x800 minimum layout.
  Visual review found small envelope knobs and an unfilled keyboard width; both
  layouts were corrected and the app rebuilt. Build receipt: `rack_build_receipt.json`.
- DSP qualification: `../build/modular-qualification.log`; native receipt and PNGs:
  `../build/ui-review/`. Importer ResourceWarnings are inherited file-handle warnings.
- Core benchmark remains the existing two-source case: 104.17 s of audio in
  0.506 s (~0.49% of one core), excluding device converter/UI. This is regression
  evidence, not a worst-case rack deadline or a new optimisation claim.

Yan auditioned 0.0.2 and reported nice tones. Agent checks on 0.0.3 are offline/native
UI checks; user audition of the new rack remains pending. No Live verification or
new VST3 packaging/installation is claimed.

## Current direction and exact next work

Requests 17–19 (keep building; show available open-source Mutable modules) are
implemented in this rack milestone. Request 20's other-maker shortlist is recorded
in `OPEN_SOURCE_MODULE_MAKERS.md`, indexed by `RESEARCH.md`: 4ms Ensemble Oscillator
and DaisySP are the recommended next sources to review. No code from those projects
has been adopted. All 21 user requests are retained verbatim in `USER_REQUESTS.md`.

Next: a sample player that can feed the resonator or mix an attack alongside it,
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
