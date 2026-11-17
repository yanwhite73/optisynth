# Engineering log

## 17 November 2026 — First Mutable voice and contextual native editor

Requests 12–14 steer the ongoing build: Mutable-first, Spectral envelope/filter
choices, contextual module panels, and whether digital modules need new circuit
models. The answer is no for the selected DSP; original code supplies it.
Source review and intended mechanism are in `MUTABLE_FIRST_SLICE.md`.

Normal and ASan/UBSan modular runs passed: both sources, min/max control sweeps,
single shared envelope, two-source sum, release, deterministic recall, active-only
construction, finite bounded output and no callback allocations. Tested output
rates: 44.1/48/96/192 kHz. Offline two-source benchmark rendered 104.17 seconds in
0.498 seconds (~0.48% of one core) in the initial optimized run. This excludes UI,
device callback, converter and future polyphony, and is not a worst-case deadline
guarantee. Original graph tests still pass (28).

Native build correction record: first attempt exposed Projucer's headerPath being
relative to generated SRCROOT, not the .jucer. Changed our generator to resolve the
local include path explicitly; arm64-only build avoids unnecessary Intel work.
Second attempt reached Main.cpp and exposed Apple Component/JUCE Component name
collision, JUCE String::isEmpty versus STL empty, and explicit String/Identifier
conversions needed for DynamicObject properties. Paused rebuilds, read compiler
diagnostics and exact JUCE String/Identifier/var/DynamicObject signatures, then
qualified Component and made conversions explicit. No DSP changes were indicated.
Native build subsequently passed. Full regression run: 1,194 inherited core
checks, 28 graph checks, 46 modular checks, four importer tests and one renderer
timing test; 46 modular checks also pass under ASan/UBSan. Existing importer file
handle ResourceWarnings remain, with no new importer edits. Final optimized
benchmark: 104.17 seconds rendered in 0.508 seconds. Native check exercises slider
binding, JSON round-trip, byte-identical audio after recall, rejected-patch
preservation and screenshots at standard/minimum size; it opens no audio device.

Visual inspection found JUCE ASCII-literal handling had garbled arrows/dashes;
replaced those UI labels with plain wording. Gave the bass-drum panel three columns
so five controls have usable knob sizes. Repeated screenshots initially appended
to existing PNG files (JUCE FileOutputStream documented default). Corrected the
exporter to seek/truncate before writing, then repeat the native check. This was
an artifact capture issue; the updated UI code was present in the built app.

Final captures inspected after the exporter fix: bass-drum controls are enlarged,
labels are readable, minimum-size modal layout fits. Each file has one PNG header.
The native check exits successfully and reproduces its audio after state recall.
`first_voice_build_receipt.json` binds source, binary and evidence hashes. The
original 83 Optispectral source files and all 20 vendored source hashes reverified.
No original checkout edits, plugin installation, Live actions, commits or pushes.

## 17 November 2026 — Listening report deferred at Yan's request

Yan reports nice tones, with some perceived aliasing when both envelope routes
are selected. Initial source inspection identified 24-sample envelope cutoff
sampling followed by 16-sample FilterFx coefficient updates as one possible cause;
clipping/nonlinearity have not been ruled out. No cause has been measured or
confirmed. Before any DSP edits or reproduction runs, Yan requested that this be
logged and performance/sound-quality refinement deferred. Investigation stopped.
Continue module variety and routing first. Requests 15–16 are preserved verbatim.

## Requests 20–21: additional sources and regular Git backups

Recorded the maker/source/licence shortlist in OPEN_SOURCE_MODULE_MAKERS.md.
Priorities: 4ms Ensemble Oscillator and DaisySP after the current Mutable rack.
No additional third-party code adopted. Yan explicitly authorized regular commits
and pushes, superseding Optisynth's inherited restriction. Commit coherent checked
milestones; retain the independent source archive workflow. Current rack code is
still being completed and must build before its implementation milestone is pushed.

## Mutable rack 0.0.3 qualified

Requests 17–19 completed: 17 types, independent module instances, actual original
Plaits oscillators/resonator and stmlib digital SVF, contextual panels, typed input
routing and output selection. Four starter patches are playable. Full graph JSON
recalls audio exactly; old instrument import preserves active sound. Invalid edits
leave the graph intact. CPU construction follows output reachability.

Native build and full regression passed first attempt for this rack step: 1,194
core, 28 graph, 74 modular, four importer and one renderer check; 74 modular checks
also passed ASan/UBSan. Native actions checked instance independence, save/recall,
invalid cycles, dormant add/remove and control disconnection/reconnection. No audio
device opened. Visual review found small minimum-size envelope knobs and excess
blank keyboard width. Adjusted drawing proportions and keyboard key width/octave
labels; rebuilt and repeated native checks/captures successfully. DSP unchanged
since the full normal/sanitizer run. Deferred roughness investigation untouched.

`rack_build_receipt.json` binds final binary/source/evidence hashes. All 83 parent
Optispectral source files and 23 vendored Mutable file hashes reverified. No Live
interaction, VST installation or parent edits. User audition of 0.0.3 is pending.

## Git and independent backup receipt

Implementation/research milestone `c947f3c` pushed to origin/main successfully;
remote branch hash was checked against the local commit. It includes all source,
user requests, maker/licence notes, provenance and qualification receipts. Builds,
SDKs and downloaded manuals remain ignored. Future work should commit and push at
coherent tested milestones under the standing request 21 authorization.

Independent archive created and verified:
`../../backups/optisynth/optisynth-source-20261117-170823.tar.gz` plus `.sha256`;
147 files, 1,754,729 uncompressed bytes, including the app headers and saved patch.
This archive represents the implementation milestone; this subsequent receipt is
also saved in Git. All temporary build/test jobs exited; user services preserved.

## Request 22: parallel Wavestate project

Recorded Yan's input verbatim and added the Wavestate repurposing project to the
research index/status as a potential controller or hardware host. These remain
possibilities to assess using that project's hardware/interface findings. This
update records the connection between projects; no hardware or DSP work performed.

## Requests 23–24: continuity correction and current app preview

Yan corrected the agent for stopping after a side-note update despite the standing
instruction to keep building. Resume the next planned sample/excitation feature;
side notes do not replace the implementation objective. Yan then asked to see the
Mutable work so far: expose the qualified 0.0.3 rack for user inspection now.

## Request 25: user audition and contextual hardware-style UI

Yan reports the Mutable examples all seem to work. Next priority is a Moog
Matriarch-style interface whose loaded modules have familiar, specific control
layouts. Research the physical panel references before implementation. Sample
playback was selected next but has not been implemented; retain it after this UI
step. The core/routing already works and should stay stable during the redesign.

Implemented standalone 0.0.4: side-by-side coloured panels, per-module layouts,
native hardware-style knobs and faders, enlarged focus view, clickable typed
patch sockets and route tooltips. Original DSP and parameter IDs/ranges are unchanged.
Scalar controls synchronize both views and saved state. Rack selectors defer
structural replacement until the widget's callback returns. Native menu handlers
reuse graph validation; rejected connections preserve the prior graph.

Build succeeded. Initial synchronization assertion used exact double equality;
changed it to a 1e-5 tolerance (one tenth of the slider's step) with value diagnostics.
Native checks pass: separate copies, synchronized views, graph/audio recall, legacy
import, dormant add/remove, typed routing, socket indicators/tooltips and cycle
rejection, contextual replacement and all 17 panels' bounds/non-overlap. Six final
captures and the Product Design review are recorded in `../design-qa.md`. Review
caught cramped minimum-height controls and generic fader handles; both were fixed,
rebuilt and visually rechecked. No audio device opened by the checks.

`hardware_ui_build_receipt.json` binds the final native binary, app source, logs and
captures. Sources underlying the 0.0.3 DSP qualification, all 23 vendored Mutable
files and all 83 parent Optispectral files match their previous hashes. The full
DSP/sanitizer suite was not rerun for this UI-only change. New 0.0.4 process opened
at 17:38 alongside Yan's existing 0.0.3 process; neither existing patch nor Live
was touched. New interface audition remains pending; request 25 confirmed 0.0.3.

Next implementation action is immutable sample assets, off-audio-thread loading
and patch persistence, then sample playback into/alongside the resonator. Existing
roughness/aliasing work remains deferred as requested.

Independent 0.0.4 archive created and verified:
`../../backups/optisynth/optisynth-source-20261117-173953.tar.gz` plus `.sha256`;
150 files, 1,789,797 uncompressed bytes. Includes the contextual interface docs,
root design review and build receipt. This subsequent archive receipt is in Git.

Request 26: Yan's “thats good” approves the opened contextual UI direction. Logged
verbatim; retain the layout. UI implementation committed as `6c8ae41`; source backup
above is independent of Git. Sample/excitation remains the next implementation step.

## Request 27: direct patching and preset architecture

Checked current RackEditor add/remove/socket handlers and rack graph persistence.
Both interaction styles can share the existing graph. Recommend editable prewired
templates, direct module/cable actions, optional cable visibility and undo; retain
the same stable document for future agent access. Record module removal semantics
explicitly: current active-dependency removal can fail validation; convenient
reconnection is future work and must not guess through branches. The architecture
note distinguishes proposed drag/undo/group features from existing 0.0.4 menus and
starter/save/load features. Sample playback is still queued.
