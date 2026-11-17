# Optispectral

Yan's synthesiser project, formerly codenamed Partials. It starts from LinPlug Spectral's
additive engine, reimplemented in our own code, and will grow from there, starting with
spectra plus samples.

The app/plugin is **Optispectral** from version 0.1.1. The `partials/` source folder,
`.ptch` format, parameter IDs and host identifiers retain their existing names for
compatibility. Optimystery recognition must include the new product name.

- **Design, milestones and the code boundary:** `docs/design.md`.
- **How the original works:** `../reverse-engineer/docs/spectral_architecture.md`.

## Build and test

```sh
~/.local/bin/agent-workload run --owner YOUR_WORKLOAD_OWNER --kind build --purpose 'Build partials' --wait 60 -- make
~/.local/bin/agent-workload run --owner YOUR_WORKLOAD_OWNER --kind test --purpose 'Test partials' --wait 60 -- make test asan
```

## Render a Spectral preset

```sh
python3 -I tools/import_linplug.py "PRESET.fxp" my.ptch
./build/partials-render my.ptch out.wav --seconds 8 --note 48:0:2.5:100 --note 55:3:2:90
```

- **Notes:** `--note PITCH:START:LENGTH:VELOCITY`, in seconds; velocity 1–127.
- **Output:** 32-bit float stereo, peak-normalised unless `--raw` is supplied.
- **Timing:** notes land on the nearest sample. `--bpm` sets tempo (default 120);
  `--ppq-start` sets the starting song position in beats (default 0).
- **Batch import:** `python3 -I tools/import_linplug.py --batch DIR OUT_DIR` converts a folder.
- **Corpus result:** all 2,648 Spectral presets in `../reverse-engineer/corpus` convert.

`renders/` and `presets-imported/` are git-ignored, because they contain third-party presets
and renders of them.

## Status

The prototype includes spectral oscillators/filters, unison, cross-modulation, LFOs, matrix,
arpeggiator and all 13 effect types. Main Vol is post effects; compressor make-up, filter
saturation and output dynamics now follow measured steady-state levels. Gator reads host
song position. Several effect laws and voice details remain approximate; see `docs/design.md`
and `../reverse-engineer/docs/spectral_architecture.md`. Local formats are VST3, AU and
Standalone; VST3 is the primary target for future Optimystery integration.

The 9–10 November build adds an independently derived nonlinear resonator and a native
editor following Spectral's layout. All four oscillator/filter pairs, spectral drawings,
envelopes, five LFOs, 24 matrix rows, 32 arp steps and six slots of effects are editable.
Click a waveform/filter display to draw; use Save As for an edited `.ptch`, or Restore to
return to the loaded/saved patch. Edits also survive host state saves.

**Caps Lock** turns MacBook typing keys into notes while the Optispectral editor has focus:
`A W S E D F T G Y H U J K O L P ; '` starts at C3 (MIDI 48); **Z/X** changes octave.
Text fields keep ordinary typing. Turning Caps Lock off or leaving the editor releases
its typing notes. The onscreen piano and MIDI input work independently.

485 core checks, four importer tests, ASan/UBSan and Release processor tests pass.
The editor controls, save/restore and concurrent edits are tested offline. Host automation
also works with the editor closed; the 60 new continuous controls use smoothed audio-path
updates rather than rebuilding the engine. Same-type effect edits preserve their history.
The actual VST3 bundle passes offline loading, parameter enumeration, audio automation and
state recall. `build/host_qualification/` contains logs; `build/ui_qualification/` has screenshots.
Filter RM remains marked unavailable; glide, Precision and some original modulation
features remain unimplemented. Extreme polyphony/high-rate CPU overruns and full-preset
fidelity remain open. See the latest design/architecture entries for measured limits.

## Plugin (VST3 + AU + Standalone, local builds only)

```sh
~/.local/bin/agent-workload run --owner YOUR_WORKLOAD_OWNER --kind build \
  --purpose 'Qualify partials release' --wait 60 -- sh tools/qualify_manual.sh build/host_qualification
```

- **Output:** `plugin/Builds/PartialsMacOSX/build/Release/Optispectral.vst3`,
  `Optispectral.component` and `Optispectral.app`.
- **Installing:** the build does not install anything. AU installation and `auval` require
  Yan's separate approval. Live checks are user-performed.
- **Installed VST3:** Optispectral 0.1.2 installed on 11 November.
  The verified bundle is at `~/Library/Audio/Plug-Ins/VST3/Optispectral.vst3`.
  This installation has not been verified inside Live. Opti's owner confirms
  new/legacy name recognition and compatible portable recall. Its expanded-control
  profile and Rack subsets are being integrated against the final 0.1.2 registry.
- **Stress test:** the runner rebuilds the real processor test and runs 20 presets through
  phases A/B/C/D/E plus the four named presets. Phase D covers editing/UI; E covers host
  controls; Release V loads the actual local VST3 without opening an audio device.
  `plugin/Tests/StressTest.jucer` is its project source. Use separate Debug
  builds for ASan/UBSan and TSan (A/C only), as recorded in `docs/design.md`.
- **Reference comparisons:** run `sh tools/verify_offline.sh` through the workload manager
  after rebuilding the renderer. It never loads the original synth or changes its WAVs.

## Host controls and backup

`docs/host_parameters.json` records the permanent IDs, short names, ranges, units and
conditional LFO eligibility. `docs/vst3_parameters.json` records actual wrapper IDs:
621 automatable controls (620 + bypass), plus 2,080 non-automatable MIDI proxies. These
are not Live parameter indices. See `docs/optimystery_parameter_handoff.md`; Opti owns
discovery/recall integration, and Live verification remains outstanding. JUCE takes the
last automation value per block; the engine smooths over approximately 4 ms. This is
not sample-accurate host automation or audio-rate modulation.

`tools/backup_source.py` makes local, checksummed source archives without GitHub. See
`docs/source_backup.md` for included files and restore instructions. The remaining
sound work is ranked from preset evidence in `docs/feature_priorities.md`.
