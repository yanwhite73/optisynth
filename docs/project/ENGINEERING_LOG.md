# Engineering log

## 17 November 2026 — First Mutable voice and contextual native editor

Requests 12–14 steer the ongoing build: Mutable-first, Spectral envelope/filter
choices, contextual module panels, and whether digital modules need new circuit
models. The answer is no for the selected DSP; original code supplies it.
Source review and intended mechanism are in `../research/modules/MUTABLE_FIRST_SLICE.md`.

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
`../verification/first_voice_build_receipt.json` binds source, binary and evidence hashes. The
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

`../verification/rack_build_receipt.json` binds final binary/source/evidence hashes. All 83 parent
Optispectral source files and 23 vendored Mutable file hashes reverified. No Live
interaction, VST installation or parent edits. User audition of 0.0.3 is pending.

## Git and independent backup receipt

Implementation/research milestone `c947f3c` pushed to origin/main successfully;
remote branch hash was checked against the local commit. It includes all source,
user requests, maker/licence notes, provenance and qualification receipts. Builds,
SDKs and downloaded manuals remain ignored. Future work should commit and push at
coherent tested milestones under the standing request 21 authorization.

Independent archive created and verified:
`../../../backups/optisynth/optisynth-source-20261117-170823.tar.gz` plus `.sha256`;
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
captures and the Product Design review are recorded in `../../design-qa.md`. Review
caught cramped minimum-height controls and generic fader handles; both were fixed,
rebuilt and visually rechecked. No audio device opened by the checks.

`../verification/hardware_ui_build_receipt.json` binds the final native binary, app source, logs and
captures. Sources underlying the 0.0.3 DSP qualification, all 23 vendored Mutable
files and all 83 parent Optispectral files match their previous hashes. The full
DSP/sanitizer suite was not rerun for this UI-only change. New 0.0.4 process opened
at 17:38 alongside Yan's existing 0.0.3 process; neither existing patch nor Live
was touched. New interface audition remains pending; request 25 confirmed 0.0.3.

Next implementation action is immutable sample assets, off-audio-thread loading
and patch persistence, then sample playback into/alongside the resonator. Existing
roughness/aliasing work remains deferred as requested.

Independent 0.0.4 archive created and verified:
`../../../backups/optisynth/optisynth-source-20261117-173953.tar.gz` plus `.sha256`;
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

## Requests 28–29: VCV performance and actual modularity

Reviewed official VCV manuals, pinned Rack engine/Module sources and Fundamental
VCO paths, with GitHub revision metadata. Recorded SIMD/polyphony, per-sample
scheduling and cable transfer, threading costs, selective outputs, graphics caching
and separate asset loading. Compared our graph compiler, Runtime, Player and native
UI: existing reachability pruning, pre-resolved pointers and meter-area repaint are
useful foundations; graph crossfades/output conversion still need peak-load coverage.

The follow-up exposed an important distinction: independent modules and arbitrary
typed audio routing exist, but a common Note reaches all modules. Pitch/most other
parameters lack modulation ports; filter mod uses the first sample per block;
event routing, voice/global execution, polyphony and feedback are future work.
Recommend editable voice groups with default note routing plus a shared global
rack. Do not describe the current wrappers as complete VCV/hardware modules.
Full findings, implementation implications and qualification matrix are in
`../research/platforms/VCV_ARCHITECTURE_LESSONS.md`. No source adoption, benchmark, DSP change or Live
interaction occurred. Sample work remains queued with these boundaries to define.

## Request 30: sample player and resonator starters — standalone 0.0.5

Implemented first-party bounded RIFF/WAV decoding, immutable shared assets and
independent playback state per sample module. Runtime receives a prepared bank;
GraphSpec keeps its existing structure. Only reachable sample references load.
The app owns one background worker, a replaceable request/result and generation
tokens. Failed or superseded loads cannot replace the playing patch. Changed
source files fail cold recall; explicit Load WAV accepts their replacement. An
explicit reload must not reuse an older asset held by another instance.

Rack v2 stores per-node paths/fingerprints; v1 rack and instrument imports retain
their sound. PCM/float, mono/stereo and source-rate conversion are supported;
stereo is averaged to mono. The sample panel exposes file load/drop, waveform,
pitch/root note, trims, level and held looping. Two built-in generated click
starters demonstrate excitation and a parallel attack. External WAV files remain
external; portable collection/stereo/seamless loops/antialias refinement are queued.
Resource and persistence contracts: `../architecture/SAMPLE_PLAYER.md`.

120 modular checks pass normally and under AddressSanitizer/UBSan; 28 graph checks
pass. Native tests cover 18 panels, previous independent/routing/recall paths,
background loading, loop control, waveform asset, external audio recall, missing/
changed file preservation, dormant pruning, cancellation and newest-only results.
Nine captures were inspected, including minimum-size sample and parallel mixer.
No audio device opened during verification, no Live actions and no VST3 installation.
The older inherited full-core/importer/renderer suites were not repeated: their
sources remain unchanged and the modular regression suite covers the changed adapter.

Two corrections found during qualification: float storage of the new slider step
perturbed existing exact control values; changed the step metadata to double,
matching JUCE's API, and reran DSP/sanitizer/native tests. Visual QA found a narrow
string/UTF-8 ellipsis mismatch; used ASCII UI copy, clarified the MIDI root-note
display and reran native checks/captures. Final review passes in `../../design-qa.md`.
New test log: `../../build/sample-qualification.log`; native evidence:
`../../build/ui-review`; artifact/source binding: `../verification/sample_build_receipt.json`.

## Request 31: Pigments/Vital modulation option

Logged the user's wording verbatim and retained their screenshot locally under
`docs/references/pigments-modulation-user-20261117.png`. Proposed source selection/
drag-to-control assignment, knob depth indication and matrix overview can share
stable node/parameter identities with module routing. `../ui/CONTEXTUAL_UI.md` records
the proposal and outstanding scope/rate/scaling contracts. No modulation matrix
implementation or product-internals claim is made. The sample milestone continued.

Delivery: new 0.0.5 app opened with `--sample-demo` and audio off (PID 85896);
existing user instance preserved. Signature/version and all 83 original parent
source hashes plus 23 vendored Mutable files verified. Independent archive created
and restore-verified: `../../../backups/optisynth/optisynth-source-20261117-182753.tar.gz`
with `.sha256`, 157 source files, 1,866,238 uncompressed bytes. This subsequent
backup receipt is retained in Git. No temporary build/test jobs remain after cleanup.

## Request 32: next priority is the complete Spectral engine

Captured verbatim while finishing the sample delivery. Sample milestone committed
and pushed as `d2e1a3a`. Inspected Engine/Patch/live-controls, plugin processor/editor
and modular execution/graph interfaces. The core already supplies full stereo
instrument playback and realtime scalar controls; actual LFO/envelope values are
private per-voice state. Existing modular output is mono and routes target ports,
so complete integration requires stereo/state boundaries and explicit modulation
scope. `../architecture/SPECTRAL_ENGINE_INTEGRATION.md` records the concrete next implementation
order and acceptance. No whole-engine/matrix runtime change is claimed yet.

## Requests 33–35: sequencer models, independent arp, delay/reverb research

Captured all three requests verbatim on arrival. Read VCV SEQ 3/timing, Impromptu
Foundry/reset/interchange, Bogaudio addressing and Music Thing evolving-loop
documentation. Inspected local Optiseq docs, current engineering/backlog entries
and relevant state/generation/playback sections read-only. Saved the comparison
and proposed event/clock contracts in `../research/sequencers/SEQUENCERS_AND_ARPEGGIATORS.md`.

Optiseq offers useful musical state, non-destructive performance transforms and
prepared playback banks; its host truncation fix still needs actual Live recall
verification. Apply versus New Variation must be explicit, and a Turing model is
not already implemented there. The inherited Spectral arp emits event batches
without sample offsets; it cannot be exposed unchanged as sample-accurate modular
event routing. No changes to Optiseq or the compatibility engine were made.

Checked primary author repositories, manuals and licence/source excerpts for
Clouds/Parasites, 4ms DLD, Plateau, CHOW Matrix, Airwindows and DaisySP. Recorded
licences, source links and outstanding review in `../research/modules/OPEN_SOURCE_DELAY_REVERB.md`.
Useful corrections: CHOW Matrix's main code is BSD-3-Clause; ReverbSc is in the
LGPL-2.1 DaisySP collection; Clouds reverb contains 32 kHz assumptions; Parasites'
current manual uses Miverb while its source still names Oliverb.

This is a documentation milestone only: no third-party installation/build/use,
no new DSP qualification, no audio device or Live interaction. Updated STATUS,
ARCHITECTURE and RESEARCH without changing the whole-Spectral next priority.

Documentation diff check passed. Independent source/research archive:
`../../../backups/optisynth/optisynth-source-20261117-184511.tar.gz` plus `.sha256`;
160 files, 1,898,938 bytes, archive checksum and every manifest entry verified.
This receipt is recorded after the archive was created. No DSP tests were rerun
for this documentation-only change.

## Request 36: whole Spectral, stereo rack and modulation matrix — 0.0.6

Captured “ok start building” verbatim on arrival. Implemented the already ordered
whole-Spectral milestone while retaining the later sequencer/arp/effect requests.
The full first-party engine is one module; its patch state, internal matrix, arp
and effects remain intact. Added explicit channel counts, stereo runtime/player/
conversion, downmix/pair/mixer adapters and control-output-to-parameter routes.
There are 23 module types and eight starters. One new starter uses an external LFO
on Spectral cutoff; the other passes Spectral audio and an actual internal LFO to
the Mutable resonator. Read-only modulation exports follow the newest active
voice, including release; the core synthesis path itself is unchanged.

Every reachable whole-engine preparation uses the existing single worker, even
when tables are cached. Dormant engines have no DSP/tables. At most two are active
per plan; independent state and compatible immutable tables are separate. Preset
import and v3 rack state preserve the full patch plus scalar overrides. Existing
rack formats remain readable. Missing, malformed and superseded preparations
preserve the playing patch. Controls/depth use atomics; structural route changes
validate and crossfade prepared plans. The shared keyboard remains monophonic;
timestamped modular events and sample-accurate independent arp are not implied.

Qualification: 1,194 inherited core checks, 28 graph checks, 151 modular checks
normally and under ASan/UBSan, four importer tests and one renderer test pass.
Exact adapter/direct L/R equality includes filtered three-unison, internal arp,
delay and release. Stereo conversion at 44.1/48/96/192 kHz, route reachability,
cycles/invalid targets, state independence and zero render allocations are covered.
Simple cold preparation measured 1.61 ms; two seconds each of filtered adapter and
direct reference rendered in 0.129 s combined. These are small fixtures, not a
dense-polyphony or transition-memory qualification. Log: spectral-qualification.log
under build/. Existing importer ResourceWarnings are unchanged, not test failures.

Native checks cover all 23 panel bounds, older rack/sample regressions, complete
stereo recall, actual live depth sliders, knob/state synchronisation, preset worker
loading, missing-file preservation and cancellation. UI review found oversized
control counts made tiny Spectral knobs at 1120x800; six-control pages fixed it.
Review also exposed an appended matrix PNG (the snapshot stream did not truncate)
and an initial zero-valued LFO shape label that needed a text refresh. Corrected
both and rebuilt/rechecked the native app. Details and captures: design-qa.md and
build/spectral-ui-review/. No audio device was opened by checks.

The final resonator screenshot also exposed an incorrectly dimmed exported LFO
rate and unlit modulation socket. Included external assignments in panel/socket
state, added a native assertion and rebuilt/rechecked. All final captures passed
visual review. Verified all 83 parent source hashes and all 23 pinned Mutable
files unchanged; source/binary/test/capture hashes are in spectral_build_receipt.json.

Remaining boundaries: contextual live controls are available; full structural
spectrum/arp/routing editing is not yet ported. No new sequencer or third-party FX
code, VST3 packaging/install, or Live interaction. Next: define timestamped event/
transport ownership and implement the first independent step sequencer/arp.

Delivery: signature verification passed and 0.0.6 opened with `--spectral-demo`,
audio off (PID 20285); prior user instances were preserved. Independent archive:
`../../../backups/optisynth/optisynth-source-20261117-191513.tar.gz` plus `.sha256`,
165 source files, 1,978,641 bytes. Checksum and every manifest entry verified, then
restored into a fresh directory successfully. This subsequent delivery receipt is
recorded in Git after creating the archive. Temporary build/test jobs completed.

## Request 37: successful general audition; timing foundation

Recorded “all seems to work ok” verbatim for the delivered 0.0.6 standalone.
Keep the qualified app available and begin the already ordered sequencer/arp
foundation offline. This feedback is not an exhaustive preset/performance test
or a VST3/Live result. The timing contract and success criteria are documented in
SEQUENCERS_AND_ARPEGGIATORS before implementation.

Implemented `timing.h/.cpp`: audio-owned sample clock, musical phase and meter,
explicit discontinuities, independent grid divisions, fixed ordered event blocks
and source/note ownership. Rechecked VCV timing and VST3 ProcessContext docs;
neither their code nor a host adapter was adopted. Timing commands are validated,
release precedes attack, and invalid/overflowing batches clear the affected bus
instead of losing note-offs. Held-note overflow remains muted until a fresh block.

47 timing/event checks pass normally and under ASan/UBSan on the first run.
Coverage includes noninteger tempo/loop boundaries, preroll, 3/4 and 7/8, changing
tempo, stop/resume/seek, independent divisions, exact event agreement across six
sample groupings, ownership/removal, capacity failures and zero playback allocation.
Log: `build/timing-qualification.log`. Added `make test-timing` / `asan-timing` and
included the timing suite in `make test`. Existing audio sources and the app were
not changed or rebuilt; their older suites need no repeat for this isolated code.

Remaining work is explicit: wire event inputs into Runtime/Player, render up to
each timestamp, prove release on stop/removal/rerouting, then expose step/control
sequencer and independent arp modules. No new audible feature is claimed yet.

Independent backup: `../../../backups/optisynth/optisynth-source-20261117-205148.tar.gz`
plus `.sha256`; 168 files, 2,006,332 bytes, archive checksum and every manifest
entry verified. This later receipt is recorded in Git. No app restart/installation,
audio device, Live action or third-party dependency was used for this milestone.

## Request 38: research before further sequencing implementation

Recorded the request verbatim and paused implementation. The earlier shortlist
and timing prototype did not provide the model/UI comparison Yan now requires.
Expand primary-source research into a taxonomy of sequence state, generation,
traversal, note transformation and arrangement, then recommend shared modes versus
separate modules. Inspect source/licences and manuals without running, installing
or adopting third-party code. Keep the timing prototype provisional.

Research complete: SEQUENCER_MODELS_RESEARCH.md categorises 15 families and separates
stored representation from view, traversal, generator, live process, note effect
and arrangement. Source cards identify inspected functions/sections rather than
claiming complete reviews from downloads. Fourteen pinned repository inventories
and 86 source/manual/image files (2,157,919 bytes) are collected locally; every
file hash checked against sequencer_research_sources_20261117.json. Three original
UI diagrams/screenshots were viewed. No third-party program or build script ran.

Findings that changed or sharpened the plan: PER|FORMER's Euclidean generator writes
through a sequence builder, distinct from a live generator; Stochas's weighted
single selection differs from independent probability; Kria parameter failure can
hold a prior value; Marbles requires memory/history as well as a random seed;
Bespoke stages dwell for pulses rather than subdividing a step like ratchets;
Meadowphysics uses explicit rule/reset passes; Seq separates reusable definitions
from playback instances and bounds zero-time graph traversal. These support the
hybrid proposal in SEQUENCER_UI_ARCHITECTURE.md and its concrete shared-UI matrix.

Licence/source corrections: PER|FORMER software is MIT, hardware separately
licensed; Mutable Grids is GPLv3-or-later while inspected Marbles headers are MIT.
Current Stellare repo/listing is not verified reusable source. Tidal's GitHub
snapshot points to Codeberg and is treated as historical evidence. Full chosen
dependency/docs/side-effect review remains required before adoption. Research is
not a benchmark or host verification.

Reconciled STATUS, ARCHITECTURE, RESEARCH, CONTEXTUAL_UI and the old sequencer doc.
No runtime, tests, app or plugin changes. Before the next build, specify the typed
Pattern contract, complete the chosen source review and compare static contextual
wireframes; the existing timing prototype does not decide the architecture.

Verification: documentation links resolve locally, JSON parses, all repository
inventories report complete trees, every collected file matches its recorded hash,
and `git diff --check` passes. No DSP tests were repeated for documentation-only
changes. Independent source archive:
`../../../backups/optisynth/optisynth-source-20261117-212559.tar.gz` plus checksum,
171 files / 2,117,334 bytes, verified and restored into a fresh directory. The
git-ignored research collection is also preserved independently in
`../../../backups/optisynth/optisynth-sequencer-references-20261117-212558.tar.gz`
plus checksum; all 103 archived reference/manifest files verified. These receipts
postdate archive creation. Temporary collection and archive jobs finished normally.

## Request 39: explicit UI research coverage

Recorded the question verbatim. Verified the saved three visual references and
existing interface analysis. Clarified that the screenshot collection is currently
an initial sample; the broader algorithm/source catalogue is not a completed visual
survey. Added direct image links and a concrete remaining panel/editor shortlist
to SEQUENCER_UI_ARCHITECTURE. Visual workflow comparison now explicitly precedes
wireframes in STATUS. No implementation, third-party execution or new build.

## Request 40: documentation folders and navigation

Moved 37 flat documentation files into project, architecture, UI, topic-based
research, verification and inherited Optispectral folders. Added a central index,
a visual-research index with direct screenshot/diagram links and an archive note.
Updated current Markdown links, relative reference paths, AGENTS and six helper/
qualification path consumers. Historical JSON and inherited document contents
remain byte-identical; all 40 verbatim request blocks were preserved. The move map
and original hashes are in document_locations_20261117.json. Raw reference assets
remain in their existing ignored collections so their provenance paths stay valid.

No sequencer/DSP feature, plugin installation or app rebuild. Wider screenshot/
interaction research remains next before the proposed wireframes and implementation.

Verification passed: 62 local links across 22 current Markdown files resolve;
37 old locations are gone and their new locations exist; all historical JSON and
legacy contents match the pre-move hashes; 40 verbatim request blocks are intact;
updated Python helpers parse and their JSON/document paths resolve; all current
JSON parses and all 86 collected reference hashes still match. `git diff --check`
passes. Qualification C++ edits only change two registry paths and one doc comment;
no DSP test/build was needed for these path-only changes.

Independent source/docs backup:
`../../../backups/optisynth/optisynth-source-20261117-213956.tar.gz` plus checksum;
175 files / 2,140,615 bytes, archive and every manifest entry verified. This receipt
postdates archive creation. The raw reference collection remains covered by the
separate reference archive recorded for request 38. Temporary validation/backup
jobs completed normally.

## Request 41: substantive sequencer visual and workflow evidence

Yan identified that the three-image sample was insufficient to establish a useful
UX paradigm. Recorded the correction verbatim and expanded the collection now.
The Product Design research workflow was applied to published reference material;
this is not a live application audit. No third-party executable or build script
was run, and no audio/MIDI device or Live session was used.

Collected 36 additional publisher assets and retained the previous three, yielding
39 images across eight source groups. Visually inspected every image: 20 software
screenshots/panel captures and 19 manual screen examples/diagrams. The originals
are under `../references/ui/sequencers_20261117/`; provenance, hashes, per-image
observations and proposed adaptations are in
`../ui/sequencers/visual_sources_20261117.json`. The eight illustrated source pages
and atlas show the actual images beside their annotations, not just external links.

`../ui/sequencers/WORKFLOWS.md` compares note entry, chords, property overlays,
independent loops, control curves, stages, Euclidean/map generation, evolving memory,
counters, arp input, queued launch and composition. It separates image observations
from manual-confirmed behaviour and our proposed desktop interactions. Expanded
screens support a shared frame with specialised central editors, explicit
sequence/track/step scope, separate selected/playing/pending markers, dedicated
performance controls and native process feedback. The architecture proposal and
current status now incorporate those findings.

Limits remain explicit: these are published stills rather than complete recorded
workflows. Exact pointer feel, keyboard accessibility, minimum size and queued/error
states need our own wireframe evaluation. No synth implementation, test changes,
plugin installation or new build. Raw images remain separate from app assets/Git;
documentation and the provenance catalogue are tracked.

Verification passed: all 39 image byte counts, file signatures and SHA-256 hashes;
all per-image annotation/source links; 201 local Markdown links; current JSON;
all 40 prior verbatim requests unchanged plus the exact new request; documentation-
only change scope; and `git diff --check`. No DSP tests were repeated.

Verified independent visual archive:
`../../../backups/optisynth/optisynth-sequencer-ui-references-20261117-215438.tar.gz`
plus checksum, containing 50 image/analysis/catalogue files (9,983,753 bytes).
Every archived member was compared with its source bytes; archive SHA-256 is
`65cb0786a4fed1b09256436bd43f6f563c468b8f6e21fa5da0d6a9d0bce082f5`.
Verified source/docs archive:
`../../../backups/optisynth/optisynth-source-20261117-215438.tar.gz` plus checksum,
186 files / 2,237,919 bytes. These receipt paragraphs postdate archive creation.
The managed validation/archive job completed normally; no temporary service remains.

## Request 42: OXI ONE model and interface reference

Recorded “you should include the ox sequencer” verbatim. Interpreted OX as OXI ONE
and stated that working identification. Studied the official MKII HTML manual,
labelled firmware 2.2.10 / manual v2.0a, keeping its features distinct from MKI.
Saved 13 chapter snapshots and 15 original diagrams; visually inspected and
annotated every selected image. The atlas now holds 54 images across nine source
groups. Source URLs, byte counts, hashes and selected inspection scope are in
`../ui/sequencers/oxi_sources_20261117.json`; the illustrated page is `oxi.md` there.

The model assessment in `../research/sequencers/OXI_ONE_REFERENCE.md` distinguishes
Mono/Poly/Multitrack/Chord, stochastic generation, independently sequenced parameter
lanes, trigger-driven note advancement, variable-duration stages and file playback.
It adds explicit chord semantics, advance-source/cursor state, stage duration,
visible arp input/harmonic following and persistent performance-state indicators
to the proposal. The current manual's Poly accumulator table/text disagreement is
recorded rather than silently resolved by assumption.

The manufacturer's ONE release repository does not establish reusable engine source
or a licence; its public E16 SDK is a different product. OXI is a commercial UX
reference alongside the open-source survey. No firmware/app was installed or run,
no MIDI/audio connection or Live action occurred, and no synth implementation changed.
The first urllib page download received HTTP 403; public curl HEAD checks succeeded,
and bounded curl downloads saved the same official pages and images successfully.
All download jobs exited and cleaned up. No browser controller was started.

Verification passed: 54 total image hashes, 15 new WebP signatures and annotations,
13 manual snapshot hashes, 159 local links, JSON parsing, all 41 prior verbatim
requests unchanged plus request 42, documentation-only change scope and
`git diff --check`. No DSP tests repeated for this documentation change.

Verified OXI reference/analysis archive:
`../../../backups/optisynth/optisynth-oxi-references-20261117-220809.tar.gz` plus
checksum, 46 files / 1,815,961 bytes; every archived member matched its source.
SHA-256: `27d87e6a1474e3b7492c1b41f520c7c2ecaecfc008353a5e8ae862486163fad8`.
Source/docs archive: `../../../backups/optisynth/optisynth-source-20261117-220809.tar.gz`
plus checksum, 189 files / 2,287,116 bytes verified. These receipts postdate archive
creation. The validation/archive job completed normally.

## Request 43: first playable sequencers — 19 November 2026

Implementation follows the completed model and visual study. Added first-party
Pattern (16 stored notes), Euclidean (up to 32 generated steps) and Stage (eight
variable-duration Repeat/Hold/Rest stages), with distinct contextual editors.
No unreviewed upstream sequencer code was adopted. The existing SampleClock,
GridClock and bounded event/ownership ledger now drive real runtime note inputs.
Player owns shared transport; receivers split processing at note offsets. Native
editors poll audio-published playheads and never schedule notes from UI timers.

Contract and acceptance were recorded before source edits in
`../architecture/FIRST_SEQUENCERS.md`. Note sockets support fan-out, including an
oscillator and its envelope; unpatched notes retain keyboard fallback. Each
connected destination remains monophonic. Stop/mute/discontinuity releases and
5 ms graph retirement keep note ownership local to its route. Rack v4 adds saved
tempo while retaining v1/v2/v3 imports. Playback/selection state is transient.
Three audible starters demonstrate the different editors. Spectral's internal
arp still has its own tempo; no claim of shared song-phase synchronisation.

Initial normal qualification passed 194 modular checks (43 new), 1,194 core,
28 graph, 47 timing/event, four importer and one renderer checks. Native app
build succeeded. All 194 modular checks also passed ASan/UBSan. Native checks passed
for all three contextual editors, full native state/audio recall, v3 compatibility,
invalid tempo rejection and Player transport. The first visual review found a
cramped Euclidean ring at 1120 x 800; its controls were moved alongside a larger
ring. Stage cells received a readable minimum width with extra width weighted by
duration. A final UI fix masks the stale audio playhead immediately on stop, even
when no subsequent audio callback occurs. Rebuilt native checks passed again.
The UI review and final hashes are recorded separately. No Live action or plugin
installation; no audio device was opened by native qualification.

Added four final assertions for real runtime note edges at 123 BPM, deliberately
off the 24-frame quantum boundary and rendered with two callback partitions.
Both starts and releases land at the expected sample. The expanded 198 modular
checks pass normally and with ASan/UBSan. Visual inspection also caught colliding
Pattern pitch/velocity labels in the shortest rack cards; their two text rows
now scale to cell height. The UI-only qualification helper rebuilds and captures
the native app without repeating unchanged DSP tests.

Final binary/source/capture hashes and receipts:
`../verification/sequencer_build_receipt.json`. Three native study racks are in
`../../patches/`; all seven sequencer captures were visually checked across the
layout review. Verified local backup:
`../../../backups/optisynth/optisynth-source-20261119-122718.tar.gz`, 201 source/docs
files, 2,368,815 bytes, SHA-256
`215531c9bfc214a4740e549ddbb3ebaca0ef176a025e5e05478be4ca7aa054b7`.
This receipt paragraph postdates the archive. All owned build/test/backup jobs
exited normally; no temporary app or service is left running.

## Request 44: project review and ambient / analog-pad goals — 19 November 2026

Claude session, running alongside the Codex session that delivered request 43
(`73ac3cc`). Request 44 was recorded verbatim on arrival. It asked for a project
review and added these goals: Rich / Roach ambient, psychedelic Berlin School
sequencing; organic, evolving sounds; Oberheim / Prophet-style pad swells.

Artist evidence checked online and cited in `SOUND_GOALS.md`:
- Roach calls the Xpander his "life time instrument" and credits its matrix
  patching.
- Rich calls just intonation "a basic part of my music" and says the Prophet-5
  was his primary keyboard synth for decades.
- Secondary claims are marked as such. Roach's use of a Prophet-5 was not
  established.

The review read the source statically. Main findings:
- The rack is monophonic and has no MIDI input.
- 13 of our own effects (reverb up to 50 s, delays, chorus, phaser) exist in
  `src/core/effects.cpp`, but only the filter is exposed as a rack module.
- Matrix and filter modulation is sampled once per 24-frame block.
- There are no random or drift modulators, no noise source and no tuning
  tables. Envelopes stop at 8 s attack and 12 s release.
- Output protection is a hard ±0.95 clamp.

A proposed order of work is in `PROJECT_REVIEW_20261119.md` and awaits Yan's
confirmation. No tests or builds were run: the Codex session held the shared
`build/` directory and the heavy-workload slots were full. Documentation only;
no code changes.

## Request 47: schematics, plugins and open-source survey — 19 November 2026

Yan asked how much circuit information, how many plugin versions and how many
open-source versions exist for the Oberheim and Prophet reference synths. The
survey used web search plus GitHub API reads of MAME file headers and component
usage. Nothing was downloaded, built or run.

Results are in `../research/instruments/OBERHEIM_PROPHET_SOURCES.md`:
- **Schematics:** service manuals and schematics exist for the SEM, OB-X,
  OB-Xa, OB-8, Xpander/Matrix-12 and Prophet-5 Rev 1/2/3. None were found for
  the current Prophet-6, Rev4 or OB-X8.
- **MAME:** BSD-3-Clause models of the CEM3310, 3320, 3340 and 3372, a Prophet-5
  Rev 3.0 driver and a partial Xpander voice board (filter and pole mixer, no
  VCOs yet; last change 27 October 2026). Its CEM3320 drive constant is
  explicitly experimental.
- **GPL-3 code:** OB-Xf, Odin 2 and GliGli's Prophet-600 firmware. Adopting any
  of them depends on Optisynth's licence, which is undecided.
- **Shruthi 4PM:** the board page (read) confirms the Xpander pole-mixing
  technique and lists its 15 modes (CC-BY-SA-3.0).

Corrections made while checking:
- The first Shruthi link pointed at the SVF2 board; replaced with the 4PM page.
- The Native Instruments URLs for Zavalishin's book now return 404; the author's
  KVR thread is cited instead.
- GitHub does not detect a licence for Odin 2 or p600fw; their GPL-3 status
  comes from the project pages.

## Request 53: inconsistent app delivery — 19 November 2026

Failure: after native build-tool startup problems, Codex compiled 0.0.8 into
`build/motion-native` and handed that temporary path to Yan while the established
Release app remained 0.0.7. The earlier version confusion (request 45) already
showed why this was unacceptable. Cost: Yan had to track changing links and
versions and correct the workflow again. The environment failures did not require
changing the user-facing location; that was an agent delivery error.

Correction: retain the existing Release app location for every handoff. Normal
builds now stage separately and use `tools/deliver_app.py`; fallback builds must
use the same helper. It checks signatures and copied executable identity before
moving the previous app to a dated archive, then writes an adjacent version/hash/
qualification receipt. The 0.0.8 signed preview is now at the stable location;
0.0.7 is archived. No running app was launched, quit or restarted. Native UI
qualification remains pending; moving the bundle does not change that status.

Prevention: AGENTS now specifies the sole delivery path, shared helper, rollback
and truthful test-status rules. README and STATUS use that path. User corrections
are recorded verbatim on arrival (including “DISCIPLINE” and the logging question),
but the request is fulfilled by the workflow correction, not merely its log entry.

## Request 54: modular sequencer depth — 19 November 2026

Failure: despite the saved source/manual/visual research, the earlier delivery
treated additional step controls and DSP checks as sufficient progress toward a
sophisticated modular sequencer. Yan rejected both the result and the vague
promise to improve it. Cost: repeated direction and audition cycles without the
requested multi-part performance workflow. That product mismatch is established
by the feedback; no claim is made that testing proves a musical result inspiring.

Correction: implemented a separate four-track Performance module from the
PER|FORMER/Foundry workflow studies. It has eight 64-step patterns per track,
sixteen chain rows, boundary launches/cancellation, edit/play/queued separation,
ties/slides/ratchets/conditions, mute/solo/reset/fill/transpose, preview/apply with
protected steps, scoped undo and clipboard. Four independent event buses and
bend/gate/CV outputs reach real destinations. Two four-part starter racks make
these behaviours available immediately. Existing specialist modules remain.

The architecture avoids thousands of global scalar slots: immutable bank payloads
are prepared/published off-thread and retired on the message thread. Event buses
are preallocated per output. Rack v5 serializes inactive banks as well as active
ones; previous racks remain readable. VA/sine accept bend for tied pitch changes;
whole Spectral does not yet accept that input.

Qualification: 251 modular checks pass normally and under ASan/UBSan. Tests cover
independent outputs/rates, exact launch/cancel/chain boundaries, stop/start bank
retention, note release, ties/glides, callback partitions and zero render
allocations. Both starters reproduce identical twelve-second stereo audio after
JSON recall; every solo track produces audio, and 24-second chains are rendered.
These are offline measurements, not listening or worst-case polyphony benchmarks.

Desktop startup remains unavailable: the standalone aborts before application
entry, with the same macOS service/account limitations seen previously. Reading
JUCE startup source identified a useful test boundary: initialize message handling
and instantiate real editor components without NSApplication/a native window peer.
That works. Native interaction checks and four actual component captures pass.
Visual review caught clipped final chain rows and cramped minimum-size settings;
both were corrected and rechecked. This evidence does not qualify full window
launch, audio-device interaction or physical keyboard feel.

The signed 0.0.9 binary is delivered through the shared helper to the sole stable
app path; 0.0.8 is archived. Exact evidence is in
`../verification/performance_build_receipt.json` and `../ui/PERFORMANCE_REVIEW.md`.
All project sources were compiled; unchanged JUCE objects were reused through the
documented direct compiler fallback. The saved receipt's source hashes were
checked against the finished worktree. No Live action, plugin installation or
user-process restart occurred.

Final source review also cleared the optional bank payload when replacing a
Performance node with another module type; otherwise dormant module replacement
would fail validation. The app was rebuilt after this change. Engine/editor code
and the reviewed captures are unchanged. The native checker now verifies source
hashes and selects exactly the generated project's objects, avoiding stale object
files after a source-list change. Own UI PNG evidence is included in source backups.

Prevention: AGENTS now requires explicit musical operations drawn from the saved
references, actual editor interaction/normal-and-minimum visual evidence, and
honest boundaries between implemented and remaining models. Do not close a product
request merely because a feature list or engine test count increased. Feedback is
retained verbatim under request 54 and this correction is bundled with its code.

Git preflight: main at `f003919` includes the concurrent classic-synth research;
those source studies are preserved. GitHub DNS still fails both inside and outside
the sandbox. Retry the authorised push and verify the remote head when connectivity
returns; do not force-push. Local snapshot/commit receipts follow below.

Verified independent source backup:
`../../../backups/optisynth/optisynth-source-20261119-145800.tar.gz` and its
SHA-256 sidecar, 234 files / 4,788,710 uncompressed bytes. Includes source, patches,
documentation and the four native UI captures; excludes build products and
upstream reference downloads. This bookkeeping paragraph and the UI index link
postdate that snapshot and are retained in the bundled Git commit.

## Request 55: scrolling changed faders — 19 November 2026

Yan reported scrolling changes faders, said the result is still not exciting,
and required a commit. The complete feedback is recorded verbatim. The previous
sequencer implementation was committed as f92326f; this session verified remote
main 4126d4d contains it. The earlier push failure was real, but the source is now
on the remote through the intervening session's push.

Cause: standalone sliders retained JUCE's enabled-by-default wheel editing.
Navigation could therefore change parameters under the pointer and alter the
sound during audition. JUCE Slider::mouseWheelMove forwards to its component
parent when setScrollWheelEnabled(false) is used. This documented/source-checked
path now applies to all eight slider construction paths: module controls,
Performance/Pattern/basic-sequencer fields, matrix amount/row depth and rack
tempo/gain. Deliberate drag and numeric edits retain their existing handlers.

Prevention: AGENTS requires this navigation behaviour for new controls. Native
checks deliver wheel events in four directions to actual slider components and
assert both unchanged values and forwarding to the containing view. Existing
editor value-entry/undo/launch checks run alongside them; unchanged DSP renders
are skipped using the checker's explicit --ui-only option. Version 0.0.10 uses
the existing stable delivery helper and archives the previous bundle.

The musical/product complaint remains unresolved. Do not interpret a wheel fix,
commit or test count as acceptance of the sequencer's depth or appeal.

Result: 758 native sliders passed four wheel directions without changing values
or swallowing navigation. Existing native editor interactions pass. The signed
0.0.10 app is delivered at the established Release location, with build/source/
delivery evidence in `../verification/scroll_fix_build_receipt.json`. No user's
running app or Live session was stopped, restarted or driven.


## Request 59: separate research-led sequencers — 19 November 2026

Yan asked to see much more of the collected UI references and clarified: “they
can be separate sequencers”. Both messages were captured verbatim on arrival.
The response is two distinct contextual devices: Probability (Stochas/OXI pitch
choice grid) and Orbit (Bespoke four Euclidean rings), with original implementation
and drawings. Contract: `../architecture/REFERENCE_SEQUENCERS.md`; native visual
mapping: `../ui/REFERENCE_SEQUENCERS_REVIEW.md`.

Each uses existing named scalar storage, clock and typed event/control outputs.
Probability fits in 736 of the existing 768 parameter slots; no global storage
increase or patch format change. UI batch edits use the existing transaction
revision. DSP has instance-local randomness, bounded event work and no callback
allocation. Orbit preserves phase while muted/soloed and has independently routed
ring notes/gates/accents. Its pending pitch is separate from its held pitch, so
editing pitch cannot change the identity of an old note release.

307 modular checks pass normally and with ASan/UBSan, including exact attack/
ratchet/swing timing, weighted selection, callback-size independence, stops,
independent ring clocks/outputs and allocation-free rendering. Both starters
round-trip complete JSON state and reproduce identical four-second stereo audio;
each Orbit ring is audible in isolation. Native interactions cover paint/select,
rotation, grouped undo/redo, snapshot, seed, protected preview/apply and capture
of actual played choices. 863 sliders pass wheel value/propagation checks.

Five actual native component captures were inspected at normal/minimum sizes.
Garbled dashes, internal numeric clock labels and missing footer controls in the
first images were corrected. Explicit footer visibility and minimum-bounds checks
now prevent that omission from passing. No desktop window/audio-device/listening
claim follows from component or render tests. All project sources were rebuilt;
unchanged JUCE objects were reused with the existing direct-build workflow.

0.0.12 was copied with the delivery helper to the sole established Release app;
the previous 0.0.11 app was archived. No user's app was stopped/restarted and no
Live session was driven. The exact delivery/source/image hashes and managed job
IDs are in `../verification/reference_sequencers_build_receipt.json`.

Lesson carried forward from the earlier disappointment: the reference must be
visible in the musical representation and editing operation, not just a research
citation beside generic controls. Keep incompatible models as separate devices
when that preserves their interaction. AGENTS' musical-operation/native-review
requirements remain in force. Dependency chains, memory models and independent
parameter loops are explicitly still separate work; tests do not settle musical
appeal. Feedback, implementation, presets and evidence are bundled together.

Git: main began with three concurrent committed pad-voice changes ahead of remote
82d32f9, plus its generated project-file edit. Those were preserved; their source
was included in build/engine qualification. No publication hold was found. Remote
access works with the approved network path. Final commit/push and independent
source-backup details are reported in the task handoff.

## Request 60 — sequencer disappears on sound changes

Instrument choices shared the full-rack loader with sequencer demos. Selecting
a sound discarded the current pattern. A second path rebuilt all rack cards on
a structural change, destroying even the open editor for a surviving sequencer.
Instrument entries now carry the edited generator and tempo by default, with
a visible Keep sequence control and separate menu headings. Sequencer windows
reopen after the matching module is rebound. Full rack loads still serve as an
explicit way to switch models. Temporary window history is session-local.

The regression check plays an edited grid through each instrument starter and
checks its saved note weights, tempo, event routing and rendered output. It also
covers Orbit and Performance through a pad. The existing offline harness had
supplied an empty sample bank, which made the sample patch silent in the test.
It now passes the same decoded sample bank as the standalone.

## Request 62 — standalone icon

The app uses a cropped detail of Yan's Lucky Dip image `lucky-dip_pattern-6.png`
and an ivory Baskerville SemiBold Italic O. The original is 7600 × 3800 pixels.
Its SHA-256 is `a23bf3492c7c3f6d1d86cda841d27fc18ec790d9f5a3db96cba7fd99dbb0099c`.
The renderer uses a 3000-pixel square at (4450, 0) and a larger letter at small
sizes. [Icon preview](../../app/Artwork/Optisynth-preview.png).

Both standalone build paths embed the ICNS before final signing. The icon was
decoded into all ten macOS representations and the 256, 32 and 16 pixel versions
were inspected. The signed 0.0.13 delivery contains the verified icon resource.

## Request 63 — generic icon on the installed app

The Dock bookmark and Launch Services both resolved the canonical Release app.
Its icon file matched the committed artwork, but NSWorkspace returned the gray
generic application icon. Touching the existing app directory and force-registering
that same path changed the system icon to the violet and mint O, verified by a
second NSWorkspace read. The installed app stayed in place throughout the fix.

The old delivery helper renamed the previous .app into the archive and installed
a new directory with the copied bundle timestamp. Updates now copy the backup,
exchange the signed Contents directories atomically, and keep the installed .app
directory identity. The helper refreshes its timestamp and registration.

## Requests 57–58: polyphonic analog pad voice — 19 November 2026 (Claude session)

Built on branch `pad-voice` in a worktree, then fast-forwarded into `main`. The
three WIP engine/app commits were pushed by the concurrent session as part of
its 0.0.12/0.0.13 deliveries, before native qualification.

Native qualification then found that the OB filter's extra knobs, VCO and
envelope had no layout: `analog.filter` fell into the generic filter-family
branch. Dedicated layouts were added. The Prophet starter peaked about 10 dB
below the OB swell; its voice-mixer level was raised.

Lesson: a module's family selects a shared panel layout. New modules need the
existing layout bounds check run before delivery. Pushing WIP commits from a
shared `main` lets another session ship them unqualified, so qualify before
fast-forwarding.

The first 0.0.11 build's `previous_app` was null. Xcode removed the 0.0.10
bundle as a stale output before the delivery helper could archive it; 0.0.10 is
rebuildable from 82d32f9. The concurrent session has since changed delivery to
keep the installed app in place (5087d57).

## Requests 64–65: pad-voice features and engineering lessons — 19 November 2026 (Claude session)

**Evidence.** Arturia XML resources (data only), Diva documentation and demangled
wavestate Native symbol names (already extracted; nothing run or copied) shaped
the design. Korg's engine design shows cost-budgeted voices, cost-balanced
voice-vector threads, level-sorted stealing, staggered starts and TPT filters.
Adopted now: sleeping voices, quietest-voice stealing, unison clusters, an
optional Steal switch, glide, quality levels, a DSP meter. Deferred with
reasons: threading, SIMD, CPU-budget stealing.

**Bugs found by checks before delivery:**
- Sleeping voices left knob smoothing stale, which produced a filter glide burst
  on wake. Fixed with a `wake()` hook that snaps smoothing.
- A woken pulse wave started with a DC step, heard as a thump through a closed
  filter. Fixed by making the pulse zero-mean at the source.
- One edit swallowed a statement into a comment, so note-offs could no longer
  find their voices. The release check caught it.

**Lessons:**
- After adding an optimisation that pauses processing, run the existing
  behavioural checks before trusting it. Every pause boundary hides state.
- Starter IDs are shared with the concurrent session's menu: map new starters
  explicitly rather than by range.

## Requests 66–70: Arturia reference comparison — lessons (19 November 2026, Claude session)

**What happened.** The first comparison passes started from each Audio Unit's
load state, which is not neutral. Arturia keeps a whole preset in the AU state:
SEM V2 loaded the arpeggiated "Acid Binge" and Prophet V3 the pad "Fascination",
both labelled "Untitled". Guessed overrides were then layered on top and probed
by trial. SEM produced silence, and wrong conclusions were drafted.

**Yan's corrections:**
- "are you starting with init patches?"
- "dont make blanket assumptions and then run tests on them. Init should give a
  good idea of what the routing needs for each synth"

**The fix.** Read the init preset first. It showed Mono/Poly, polyphony, play
mode and MIDI channel, none of which can be set through AU parameters. Then
inspect the AU state structure and load the preset whole.

**Prevention.** For any reference instrument:
1. Identify and load its init or default patch whole, and verify the loaded
   preset name in the output.
2. Read that patch to decide the test configuration, rather than overriding
   parameters whose meaning has not been read.
3. Record per-instrument facts that change comparisons, such as Prophet's
   5-voice limit.

## Request 71: preset browser, synth-panel view, rack zoom (20 November 2026, Claude session)

**Design choices:**
- **Synth panel.** It is a second view over the same parameters, not a
  second engine. Its knobs call the same `setParameter` path as the rack
  cards, so DSP handles, the cards, the focus editor and the saved patch stay
  in one place.
- **Metadata location.** Patch metadata (category, tags, view, panel layout)
  goes in the patch file. Per-user state (favourites, zoom, view preference,
  extra folders) goes in a settings file.
- **Native-check isolation.** Snapshot mode points both the settings and the
  library at the snapshot folder, and empties them first, so qualification
  cannot touch Yan's files.
- **Zoom.** It is a component transform on the rack, not a relayout. The
  card layouts use fixed pixel offsets, and the transform keeps text and knobs
  vector-sharp. The rack still lays cards out in rows at the zoomed logical
  width, so zooming out shows more modules.
- **Metadata edits.** Editing a user preset's metadata changes only `info`
  inside the existing JSON. The result is validated with the full reader and
  written through a temporary sibling.

**Found by inspecting captures** (all native checks already passed):
- The Panel / Modular radio buttons had no visible on state.
- Slow LFO rates displayed as 0.0 Hz.
- The preset table showed a horizontal scrollbar.
- Panel sections left spare height at the bottom.

All four were fixed and rechecked.

**Lesson.** Passing layout checks prove only that controls are in bounds and do
not overlap. Readability and state visibility need the captures to be looked
at, and here that look found four issues the checks could not.

## Request 72: 0.0.16 review — "embarrassingly bad" (20 November 2026, Claude session)

**User cost.** Yan installed and used 0.0.16 and found:
- the zoomed rack's text unreadable;
- no rows;
- his saved preset missing from the browser;
- arrows not working;
- the keyboard silent.

He then had to ask whether JUCE has a UI toolkit, and said: "I have to
continually prod and poke to remind you agents to use existing frameworks and
open source code" and "Why do I have to explain everything that should be
obvious?"

**Causes, with evidence:**
- **Zoom.** I scaled the whole rack with a component transform instead of laying
  out with JUCE Grid/FlexBox. Text shrank with everything else. My checks tested
  bounds and overlap only, and I judged the 65% capture acceptable.
- **Preset location.** The browser looked only in a new folder that nothing had
  been saved to. Yan's patch was in the repository's `patches/` folder, saved by
  the old Save dialog. I did not check where he actually saves.
- **Keyboard.** Reproduced with a probe. Keep sequence (on by default) wired the
  startup sequencer into the pad's voice mixer, which cut the keyboard off. The
  panel view hid the sequencer entirely.
- **Arrows.** The browser gave focus to its container rather than its list, so
  the up/down keys I had advertised did nothing. The header arrow code worked.
  A first probe wrongly reported it broken: JUCE hit-tests injected mouse and
  pen events at the real cursor, not the injected point. That is visible in
  `juce_MouseInputSourceImpl.h`.
- **Delivery near-miss.** `tools/build_app.sh` would have replaced the app while
  Yan was running it. Nothing checked for a running process.

**Prevention:**
- **Survey first.** Before any UI or infrastructure feature, write the
  existing-options note (JUCE built-ins and licence-checked open source). See
  [JUCE_UI_OPTIONS.md](../research/platforms/JUCE_UI_OPTIONS.md) and the session
  memory `use-existing-frameworks-first`.
- **Layout.** Never transform-scale for density. Text sizes are fixed
  constants, and checks assert that every control is at least as wide as its
  widest text.
- **Test as a player.**
  - Load presets the way the UI does.
  - Play the keyboard and confirm audio.
  - Press the arrows and keys.
  - Look for files where the user saves them.
- **Simulated input.** Use touch type or call handlers directly. Never trust
  injected mouse events for buttons.
- **Delivery guard.** `deliver_app.py` refuses while a process runs from the
  Release app. Builds for testing go to `build/app-candidate` only.
