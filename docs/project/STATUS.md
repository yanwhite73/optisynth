# Optisynth status — 22 November 2026

## 0.0.18 Mac keyboard and sizing controls

Installed at the pinned Release app on 22 November. Quit and reopen from the
Dock. The panel, modular rack and focused module now expose the size buttons.
Use **−** or **+** to adjust controls with fixed text sizes, or **Fit** to size
them to the available space. The panel remembers its own density.

Caps Lock note and octave presses are consumed by the instrument and the three
floating performance editors. The native suite checked the mapping and its
exceptions, control geometry and preset behaviour. Normal and minimum panel
captures were inspected. Audible confirmation on the user's Mac is outstanding.
The installed executable matched the candidate at SHA-256
`bb6a6dc6a323d4408903bd3bef2d2a93ef38d0dad8c3f06baeea3e333be6bf91`.

## 0.0.17 readable density layout, rows, preset/arrow/keyboard fixes — requests 72–73

**Installed 20 November at 10:30 UTC.** The signed 0.0.17 executable at the
pinned Release path matches the checked candidate (SHA-256
`a8a218152977ba5f21c0eb76dfa9573a5ab76d831c02ff3dc3cb645b9dd8b437`).
The Dock URL resolves to the same folder, whose filesystem ID stayed 176340452.

Future standalone deliveries install while the old session runs. Once an update is
reported ready, quit and reopen from the same Dock icon to use it (request 78).

**Changes:**
- The transform zoom is replaced by a JUCE Grid/FlexBox density layout. Text
  stays fixed; knob size and spacing change.
- Rows can be set by hand and are saved in the patch.
- User presets live in `~/Music/Optisynth/Presets/User`.
- Arrows fixed: header arrows, Left/Right keys, and browser list keys.
- The keyboard plays a sequencer-driven pad.

**Checks.**
- DSP: 1,194 core, 28 graph, 329 modular (normal and ASan) and 47 timing.
- Native: ui, sequencer, pad and panel/browser receipts.

Details: [SYNTH_PANEL_AND_PRESETS.md](../ui/SYNTH_PANEL_AND_PRESETS.md#request-72-rework-0017).
Framework survey: [JUCE_UI_OPTIONS.md](../research/platforms/JUCE_UI_OPTIONS.md).


## 0.0.16 interface: preset browser, synth-panel view, rack zoom — request 71

App: `app/Builds/MacOSX/build/Release/Optisynth.app` (receipt
`Optisynth-build.json`, native checks passed). Design and results:
[SYNTH_PANEL_AND_PRESETS.md](../ui/SYNTH_PANEL_AND_PRESETS.md).

- **Header.**
  - Preset bar: previous / next, a favourite star, the preset name with its
    category and tags, and Save.
  - Panel | Modular switch, with a menu for which view a patch opens in.
  - Transport and status on the second row; rack tools and zoom on the third
    row, in Modular only.
- **Preset browser,** replacing the Starter patches menu.
  - Collections: All, Favourites, Factory and User, plus categories with counts.
  - Search, tag chips (combined with AND) and a sortable table.
  - Details pane with metadata editing for user presets; Save to library.
  - Import / Export file, Add folder and Keep current sequence.
  - All 21 factory starters now have a category, tags and a description.
- **Synth-panel view.**
  - One front panel of the patch's playing controls, kept in sync with the
    rack cards and the audio thread.
  - The four pads carry curated layouts and open in this view. Other patches
    get an automatic panel, one section per module, with sequencers as editor
    buttons.
  - Original colour schemes: slate, walnut, ivory and graphite.
- **Rack zoom,** 50–200%.
  - Use −, + and Fit, Cmd − / = / 0, or pinch. The scroll wheel still only
    scrolls.
  - Zooming out adds rows rather than shrinking one row.
- **Storage.**
  - The patch format is now version 6, adding `info`, `view` and `panel`;
    versions 1–5 still load.
  - Favourites, zoom and the view preference are stored in
    `~/Library/Application Support/Optisynth/`.
  - The user library is `~/Music/Optisynth/Presets/`, created on first save.

Not yet: editing a panel layout by hand, or modulation depths as panel
controls. Pinch, keyboard browsing and dialogs still need hands-on use.


## 0.0.15 pad voice: 16 voices, glide, unison, tuning, Xpander modes — requests 64–65

- **Voice mixer:** up to 16 voices, unison 1–8 with detune, glide, a Steal
  switch, and tuning (Equal, Just 5-limit, Just 7-limit, Pythagorean,
  non-octave Harmonic series) with a root note.
- **OB filter:** Xpander's 15 pole-mixing modes, and quality at 1×, 2× or 4×
  oversampling.
- **Poly-mod:** a new Prophet-style module for per-voice FM, PWM and filter.
- **Voice handling:** released silent voices sleep, and stealing takes the
  quietest held voice.
- **DSP meter:** load is shown beside the transport position.
- **New starters:** Pad / Xpander mode, unison + glide; and Pad / Just-intonation
  harmonic drone. The Prophet pad now uses poly-mod.
- **Background:** the engineering choices draw on Arturia, Diva and Korg
  wavestate evidence; see
  [PERFORMANCE_QUALITY_LESSONS.md](../research/platforms/PERFORMANCE_QUALITY_LESSONS.md).
  Contract: [VOICE_GROUPS.md](../architecture/VOICE_GROUPS.md).

## 0.0.14 analog pad voice qualification — requests 57–58

Polyphonic voice groups, the analog VCO, OB filter, CEM-style envelope, OTA VCA,
Wheel / bend, stereo reverb, chorus, delay and phaser modules, and hardware MIDI
input. Contract: [VOICE_GROUPS.md](../architecture/VOICE_GROUPS.md). Starters:
**Pad / OB swell (8 voices)** and **Pad / Prophet 4-pole (8 voices)**. This build
fixes the OB filter, VCO and envelope panels, which earlier builds left
partly unplaced, and matches the Prophet starter's level to the OB swell's.

Qualified offline:
- **Checks:** 1,194 core, 28 graph, 307 modular (normal and ASan/UBSan) and 47
  timing checks, plus the importer and renderer tests.
- **Native:** UI checks plus pad starter load, display, recall and level checks.
- **CPU:** 8-voice pad with chorus and reverb at 6.5% of one core, with no
  render allocation.

Chord renders for listening: `build/sequencer-ui-review/pad-ob-swell.wav` and
`pad-prophet.wav`. No audio device was opened. Listening is Yan's.

## 0.0.13 sound selection — request 60

**Keep sequence** is on by default. Selecting an instrument starter carries the
sequencer, its saved edits and tempo into the new instrument graph. Orbit routes
its combined notes and Performance routes track one to the instrument. For a
polyphonic pad the notes go to its voice mixer. Sequencer starters and Open still
load a whole rack. A rack with multiple sequencers requires an explicit full
rack change. Rack rebuilds reopen expanded editors for surviving sequencer IDs
and models. Runtime playhead, editor undo and temporary snapshots can restart.

Yan heard the current sound starting to come together musically and called the sequencer
options a good start (feedback 61). The saved reference is `~/Music/Optisynth/Presets/User/lush ambient space.optisynth` (moved from `patches/` on 20 November, request 73),
with the Performance sequencer driving an analog pad at 118 BPM.

## Previous delivery — Probability and Orbit, request 59, 0.0.12

The single delivery app remains **`app/Builds/MacOSX/build/Release/Optisynth.app`**.
It opens **Probability / Glass branches** and its editor with audio off. Press
PLAY. Choose **Orbit / Four satellites** in Starter patches and click OPEN ORBIT
for the other device. The previous 0.0.11 app is archived by the delivery helper.

- **Probability:** 16 pitch rows ×32 steps; painted relative weights, separate
  gate chance, scale/root/octave, repeat/fresh decisions, seed, per-step velocity/
  gate/ratchets/conditions, row mute, protected preview/apply, scoped undo,
  snapshot/restore and capture of the actual played choices.
- **Orbit:** four Euclidean rings, independent clock/rhythm lengths, drag rotation,
  mute/solo, per-ring articulation, accents and signed rotation per cycle. Four
  separately routable note/gate/accent outputs plus a combined note bus.
- Two complete routed starters, with identical state and four-second stereo audio
  after JSON recall. Every Orbit ring is audible alone. No listening claim.
- **307 engine checks** pass normally and under ASan/UBSan. Actual native editor
  gestures and five normal/minimum captures pass; **863 sliders** preserve values
  and forward wheel navigation. Final source/binary hashes match the delivery.

[Controls and boundaries](../architecture/REFERENCE_SEQUENCERS.md),
[actual editor images and research mapping](../ui/REFERENCE_SEQUENCERS_REVIEW.md),
[qualification receipt](../verification/reference_sequencers_build_receipt.json).
The supplied Stochas/Bespoke/OXI references guide original implementations and
artwork; no new third-party code was adopted. There are now 39 module types and
19 starters, including the preserved pad-voice work from the concurrent session.
The pad's desktop MIDI/audio behaviour was not newly qualified here.

Desktop-window launch, audio-device interaction and listening remain user checks;
no Live action or plugin installation occurred. Existing Performance and the
other sequencers remain available. This delivery does not close Yan's earlier
musical-quality complaint by test count. Next product work: assess these distinct
interfaces in use, then implement remaining specialist models (dependency/memory
sequencing, independent parameter loops, held-note arp/chord modes) from their
specific research. Snapshot/undo history remains session-local; save the rack
for persistent patterns. No named upstream product is claimed fully implemented.

Git preflight: main had the other session's three committed pad-voice changes
(`ba09c85`, `dcb3673`, `2aea7a4`) ahead of verified remote `82d32f9`; no publication
hold was found. They are preserved and included in this build/check pass. This
request's implementation, presets, native evidence and records form one scoped
commit. The task handoff reports the verified remote commit and independent backup.

## Prior correction — request 55, 0.0.10

Scroll gestures no longer edit standalone faders/knobs: this covers module panels,
the three sequencer editors, modulation depth rows, tempo and output level.
Deliberate drag/numeric entry remains available. The stable delivery path is
unchanged. Native wheel-event checks cover value preservation and propagation to
the containing view; full desktop interaction is still a separate check.
All 758 tested sliders pass in four wheel directions; the existing native editor
interaction checks also pass. [0.0.10 delivery receipt](../verification/scroll_fix_build_receipt.json).

Yan's assessment of 0.0.9 is “Overall its not an exciting result.” The broader
sophisticated-sequencer request remains open; this fix does not resolve it.
The 0.0.9 implementation was already committed as f92326f and is now verified as
an ancestor of remote main 4126d4d. This correction is a separate bundled commit.

## Previous delivery — request 54, Performance sequencer, 0.0.9

The stable app is **`app/Builds/MacOSX/build/Release/Optisynth.app`**. It now
contains the signed 0.0.9 build and opens the Four currents performance rack and
editor with audio off. The previous 0.0.8 bundle is preserved under
`build/app-archive/20261119T145353.389402Z-0.0.8/Optisynth.app`.

Implemented from the PER|FORMER/Foundry workflow studies: four independent tracks,
eight 64-step patterns each, separate note/gate/bend/CV outputs, queued/cancellable
boundary launches, sixteen repeating chain rows, individual rates/lengths,
ties/slides/ratchets/conditions, mute/solo/reset/fill/transpose, protected variation
preview, clipboard and scoped undo/redo. The contextual editor keeps selection,
playing and queued states distinct. Four currents and Clockwork garden connect
four audible destinations and CV assignments. There are 27 module types and 15
starters. Rack v5 embeds all banks/chains and reads prior versions.

- **Engine checks:** 251 pass normally and under ASan/UBSan, including independent
  output buses, exact launch/chain timing, tied pitch motion, stop ownership,
  callback-size independence and no render allocations.
- **State/audio:** both starters reproduce identical twelve-second stereo audio
  after full JSON recall; every track produces audio when soloed. Twenty-four
  second chain WAVs are under `build/performance-*.wav`. No listening claim.
- **Interface:** real JUCE component interaction checks pass without a desktop
  window peer. Four screenshots were inspected at 1260×930 and 1120×800. The
  clipped final chain row and cramped settings in the first capture were fixed.
  [Visual evidence](../ui/PERFORMANCE_REVIEW.md).
- **Desktop/Live:** native application startup still aborts before application
  code in this execution environment. Component testing avoids NSApplication
  startup and does not establish desktop-window, audio-device or keyboard feel.
  No Live session was driven and no plugin was installed.

[Controls and source-to-feature mapping](../architecture/PERFORMANCE_SEQUENCER.md),
[build receipt](../verification/performance_build_receipt.json). The judgment of
musical usefulness remains Yan's; passing checks does not establish that judgment.

Next verification: open the stable app in the user's desktop, press PLAY and
exercise bank launching and the chain. No further rebuild is needed for this
check. Pending product work includes independent held-note arp, specialist
memory/stochastic/chord models and polyphonic destinations. The older modules
remain available; this implementation does not claim to cover those families.

Git: base `f003919` includes concurrent classic-synth research and the prior
delivery commits. The current work is bundled into one scoped commit. GitHub
lookup still fails (`Could not resolve host: github.com`) inside and outside the
sandbox. Keep the local source backup; retry the authorised push and verify the
remote branch head when DNS is restored. See ENGINEERING_LOG for the final receipt.

## Prior delivery — request 53, stable app delivery

Always open `app/Builds/MacOSX/build/Release/Optisynth.app`. It now contains the
**0.0.8 preview**, with the same signed binary as the Motion build receipt.
`Optisynth-build.json` beside it records version, hash and the pending native UI
qualification. The previous 0.0.7 app is preserved under
`build/app-archive/20261119T141532.763540Z-0.0.7/Optisynth.app`.

The normal build now compiles into `build/app-candidate`, then uses the shared
delivery helper. Fallback builds must use that helper too. It verifies the source
and copied signatures/hash, archives the previous app and preserves it on a
copy failure. No app was launched or stopped for this correction. See the request
53 failure/prevention entry in ENGINEERING_LOG and the enforced AGENTS guidance.

## Request 49 — Pattern / Motion, standalone 0.0.8

Implemented: a 64-step Pattern engine and large native performance editor with
independent pitch/velocity/CV loops, six traversals, conditions, chance, ratchets,
swing, scale/keyboard transposition, drawing, undo/redo, clipboard, protected
generation and temporary snapshots. Night current and Glass tide are original
Spectral delay/reverb starters with both CV lanes assigned. 26 modules, 13 starters.
[Feature contract](../architecture/PATTERN_MOTION.md),
[source decision](../research/sequencers/PATTERN_MOTION_SOURCE_STUDY.md).

Passed: 221 modular checks normally and under ASan/UBSan, including exact event
offsets, block-size independence, stop ownership, separate CV loops and coherent
parameter transactions. Both new presets render 12 seconds of finite stereo audio
and reproduce exactly after JSON save/reload. Presets are in `patches/motion-*`.

Native compilation succeeded through direct Clang using the existing Xcode
configuration and unchanged JUCE module objects. All project C++ sources were
rebuilt and the app was ad-hoc signed. Its internal build location was
`build/motion-native`; request 53 delivered it to the stable app location above.

**Outstanding:** native runtime/UI checks and visual inspection. The new app and
Projucer both abort during startup with no application log. Xcode also fails
before compilation, reporting file-event/cache service errors; the execution
context cannot resolve the macOS account name and cannot access logd. This is an
environment obstacle, not proof that the new interface is correct. No screenshots
or native editor PASS receipt were produced. Next: once desktop services are
available, run the new app's `--snapshot` checks, inspect all Motion captures,
correct any issues, then update the delivery's qualification record.
Launch Services also returned -10827 despite the executable being present,
0755, arm64 and signature-valid. No audio device or Live session was opened.

The full firmware adoption was declined after remaining hardware docs could not
be retrieved (DNS failure). Pattern / Motion is an independent implementation of
the researched behaviours, with no newly adopted upstream firmware code.


Delivery bookkeeping: implementation/tests/presets/docs are bundled into one
local commit. Push to `origin/main` failed because `github.com` could not resolve
from this execution context; retry the existing push when connectivity returns.
A verified local snapshot is `../backups/optisynth/optisynth-source-20261119-134854.tar.gz`
(215 files, 2,570,309 bytes; before JSON line-ending normalization). The normal
0.0.7 app is archived intact. Native qualification and remote push are still open.

Resume check (request 52, 19 November): `git ls-remote origin refs/heads/main`
still fails DNS, including outside the sandbox. `scutil --dns` reports no DNS
configuration; `launchctl print gui/501` fails with error 141, and account lookup
still returns numeric UID 501. The native snapshot command was queued through the
workload launcher for 60 seconds, then refused for host load above two tasks per
CPU; the app did not start. Do not attribute that refusal to the app or rebuild
unchanged code to address it. Next action remains native `--snapshot` qualification
of the stable delivery app, visual review, then the authorised push and
remote-head verification. Source commit `7c1f3c0` remains local-only as far as this
session could verify at that point. Resume notes are included with request 53's
delivery-workflow correction.

## Prior delivery — standalone 0.0.7 sequencers

User feedback, request 46: the first pass works but feels underwhelming. The
current technical slice is not yet the desired musical/editor experience. Next
product pass should prioritise richer evolving sequencing, more direct editing
and compelling ambient/Berlin School patches with time effects; see the verbatim
request record and `SOUND_GOALS.md`. No such enhancement is claimed complete.

Request 43 adds three playable first-party modules: Pattern (16 stored steps),
Euclidean (up to 32 steps, hits/rotation) and Stage (eight variable-duration
Repeat/Hold/Rest stages). Their contextual editors share transport and a selected
step inspector; the playing marker comes from audio-thread state. There are now
26 modules and 11 starters. Start audio, choose one of the three new starters,
then Play seq. Stop seq releases notes; Restart returns to beat zero.

Typed `notes` sockets route independent sequences, with fan-out to an oscillator
and its envelope. Receivers split processing at event offsets; unpatched notes
retain keyboard fallback. Preparation is still lazy/off-thread, and tested render
paths allocate nothing. Rack v4 saves tempo and all sequence settings; v1/v2/v3
remain readable. Play/stop and selection are transient. Contract:
[FIRST_SEQUENCERS.md](../architecture/FIRST_SEQUENCERS.md).

Qualified: 198 modular checks normally and under ASan/UBSan, 1,194 core, 28 graph,
47 timing/event, four importer and one renderer checks. Native app build and all
native checks pass, including contextual edits reaching DSP, rack/focus sync,
complete state/audio recall, stop indicators and minimum-window layouts. Final
captures: `../../build/sequencer-ui-review/`; audio fixtures: `../../build/sequencer-0.wav`
through `sequencer-2.wav`. No audio device was opened by UI checks; listening is
user-performed. The 123 BPM tests verify exact starts/releases inside a render
quantum with different callback sizes. [Build receipt](../verification/sequencer_build_receipt.json).

Limits/next: independent held-note arp; voice groups/polyphony; deeper Pattern
editing, probability, capture and arrangement. Note destinations are currently
monophonic. Spectral's internal arp retains its own tempo setting; shared beat
synchronisation is not implemented. This is the standalone, not a new modular
VST3 build. No Live action or installation occurred.

## Request 44 — project review and new sound goals (19 November)

New design targets: Robert Rich / Steve Roach ambient, psychedelic Berlin School
sequencing; organic, slowly evolving sounds; lush Oberheim / Prophet-style pad
swells. Sourced goals and requirements: [SOUND_GOALS.md](SOUND_GOALS.md). Review
and proposed order: [PROJECT_REVIEW_20261119.md](PROJECT_REVIEW_20261119.md).

Proposed next order (awaiting Yan's confirmation; it does not replace the list
above until confirmed):
1. Rack modules for the existing reverb, delay, chorus and phaser effects, plus
   Noise, Drift/Random and Slew modules and longer envelopes.
2. Hardware MIDI input.
3. Voice groups and polyphony.
4. Analog pad voice: cents detune, 4-pole and SEM-style filters, poly-mod.
5. Tuning tables.
6. Berlin School sequencer features.

Open questions: which recordings define the target; how many pad voices.

Request 47: [Oberheim and Prophet sources](../research/instruments/OBERHEIM_PROPHET_SOURCES.md)
surveys circuit documentation, plugins and open source for the pad voice.
Schematics exist for all the vintage targets. MAME has BSD-3-Clause CEM chip
models and a Prophet-5 Rev 3.0 driver. OB-Xf and Odin 2 are GPL-3, so treat
them as references only until Optisynth's licence is decided. Next: download and
hash the remaining manuals and datasheets, then analyse the Xpander voice card.
Research only.

Request 50: Optisynth is aimed at commercial release. [LICENSING.md](LICENSING.md)
lists the adopt and study-only licences, the installed Arturia and Valhalla
reference plugins, and three pre-release items: a JUCE commercial licence,
removing Mutable names from user-facing text, and a review of the Spectral name.
Documentation only: no code changed, nothing built or tested for this request.

Request 40: documentation is sorted into project, architecture, UI, research,
verification and legacy folders. [Documentation index](../README.md) provides the
entry points; [UI research](../ui/README.md) links the collected interface images.
Current agent/helper/link paths follow this structure. Historical evidence and
verbatim request text are preserved; no new synth build accompanies the move.

## Prior delivery — sequencer research and architecture proposal

Request 42 adds [OXI ONE MKII](../research/sequencers/OXI_ONE_REFERENCE.md), interpreting
the user's “ox sequencer” as OXI ONE. Fifteen official diagrams are saved, visually
inspected and [annotated](../ui/sequencers/oxi.md), with 13 manual chapter snapshots.
The combined atlas now has 54 images across nine source groups. OXI is a commercial
UX/model reference; reusable ONE engine source has not been established.
New design requirements cover typed chord progression editing, explicit trigger-
driven phrase advancement, stage duration, visible arp input/harmonic relationships
and persistent performance state. Include these scenarios in the next wireframes.
No runtime implementation or app build accompanies this research.

OXI backup: `../../../backups/optisynth/optisynth-oxi-references-20261117-220809.tar.gz`
(46 reference/analysis files verified); updated source/docs archive:
`optisynth-source-20261117-220809.tar.gz` in the same folder (189 files verified).
Checksums and validation details are in `ENGINEERING_LOG.md`.

Request 41 is addressed with an expanded [visual atlas](../ui/sequencers/README.md):
39 publisher images saved and visually inspected across eight source groups
(20 software screenshots/panel captures; 19 manual diagrams/screen examples).
Every image has an annotation, original source URL and hash. The
[workflow comparison](../ui/sequencers/WORKFLOWS.md) links them to concrete editing,
performance and arrangement tasks, with explicit observations versus inferences.
This corrects the insufficient three-image sample. No hands-on audit is claimed;
device implementation remains paused while the proposed UI/contract is refined.

Request 38 is fulfilled as research and documentation, with no further building.
[SEQUENCER_MODELS_RESEARCH.md](../research/sequencers/SEQUENCER_MODELS_RESEARCH.md) categorises 15 families
and separates representation, views, traversal, generation, live processes, note
transformation and arrangement. Fourteen pinned repository inventories, 86 selected
source/manual/image files (2,157,919 bytes), licences and inspection scope are in
`../research/sequencers/sequencer_research_sources_20261117.json`. All collected hashes verified. One
repository is a negative finding: the current Stellare listing is not verified
reusable open-source implementation. Selected inspection is not adoption clearance.

[SEQUENCER_UI_ARCHITECTURE.md](../ui/SEQUENCER_UI_ARCHITECTURE.md) proposes one configurable
Pattern module plus independent Arp, Stage, Euclidean, Memory, Rhythm Map, Counters
and Conductor families. It specifies shared widgets, contextual editors, native
state/capture, agent operations and performance implications. These are proposed
families, not implemented features or all part of the first release.

Requests 39 and 41 make visual/interface evidence a required part of this research.
The expanded comparison supports a stable workspace with specialised editors,
explicit editing scope, separate playing/selected/queued states, and visible
process state for Stage, Memory, Counters and Arp. Raw visuals remain local with
a separate backup; analysis and provenance are tracked in Git.

Historical research handoff before request 43: compare Pattern/Arp/Stage/Memory wireframes
against the documented tasks and empty/selected/playing/queued states; make the
Pattern lane/event/port contract concrete and complete the selected dependency
review. Existing timing code remains an isolated prototype to reassess against
that contract. The remaining implementation steps below are queued; the prior
standing instruction to continue did not override that research-first request.
Request 43 subsequently authorized implementation; the current delivery above
supersedes this pause for its first three modules. Other model proposals remain queued.

Research backup: `../../../backups/optisynth/optisynth-sequencer-references-20261117-212558.tar.gz`
and source/docs backup `optisynth-source-20261117-212559.tar.gz` in the same folder,
both with checksums and verified contents. The source archive also passed a fresh
directory restore. Source/raw reference copies were not run or installed.

Request 41 visual backup:
`../../../backups/optisynth/optisynth-sequencer-ui-references-20261117-215438.tar.gz`
contains all 39 images plus the illustrated analysis/catalogue (50 files verified).
Updated source/docs backup: `optisynth-source-20261117-215438.tar.gz` in the same
folder, 186 files verified. Checksums and details are in `ENGINEERING_LOG.md`.

## Prior delivery: complete Spectral and modulation matrix, standalone 0.0.6

23 module types and eight starters. New: complete `spectral.engine`, independent
LFO, Stereo to mono, Stereo pair and Stereo mixer. Full Spectral preset state,
internal synthesis/matrix/arp/effects and stereo output are retained. Existing
imported `.ptch` presets load on the preparation worker and become embedded rack
state. The contextual panel has paged live controls; structural spectrum, arp and
internal-routing editing is not yet a replacement for the old full plugin editor.

The Matrix window adds/removes control-output-to-parameter assignments and changes
depth live. Assignments keep base knob values separate and use the saved graph's
dependency/cycle validation. Spectral exports its actual five LFOs and twelve
envelopes from the newest active voice, including release. The independent LFO
offers sine, triangle, ramp and pulse, depth and optional note reset. Two new
starters demonstrate external LFO -> Spectral cutoff and Spectral audio/LFO ->
Mutable resonator. Stereo remains separate through device output; mono conversion
requires the explicit adapter. The shared rack keyboard is still monophonic.

Rack v3 adds embedded Spectral state and modulation routes, preserving older rack
and instrument imports. At most two whole engines may be active per plan; their
compatible tables can be shared. Only reachable engines prepare, always on the
worker. Scalar edits and matrix depth changes use atomics. Full contract and
remaining limits: `../architecture/SPECTRAL_ENGINE_INTEGRATION.md`.

Offline qualification: 151 modular checks pass normally and under ASan/UBSan;
1,194 core checks, 28 graph checks, four importer tests and one renderer test pass.
Wrapped/direct stereo renders match exactly, including a filtered/unison patch
with internal arp, delay and note release. Tested rendering allocates nothing.
Log: `../../build/spectral-qualification.log`. The simple unfiltered cold-load probe
took 1.61 ms; two seconds each of the filtered adapter and reference took 0.129 s
combined. These are limited fixtures, not dense-polyphony performance claims.

Native checks cover v1/v2/v3 and full stereo recall, live depth/knob synchronisation,
preset import, missing/invalid-state preservation and cancellation. Screenshots
and receipts are in `../../build/spectral-ui-review`; visual findings are in
`../../design-qa.md`. No audio device, Live session or new VST3 was used for checking.
Final source/binary/test/capture binding: `../verification/spectral_build_receipt.json`. All 83
parent source files and 23 pinned Mutable files remain unchanged.
Yan reports “all seems to work ok” after trying this delivery (request 37).
This is successful general standalone audition feedback; no exhaustive preset
or Live verification is inferred.

The next timing foundation is implemented offline in `src/modular/timing.h/.cpp`:
shared sample clock, independent grid divisions, ordered bounded note events and
source-specific held-note ownership. All 47 checks pass normally and under
ASan/UBSan with zero counted playback allocation. See `../research/sequencers/SEQUENCERS_AND_ARPEGGIATORS.md`
and `../../build/timing-qualification.log`. It is not yet connected to Runtime/Player;
no new app build or playable sequencer/arp module is claimed. Event integration
and panels are deferred until the research-derived design steps above are complete.

## Existing sample-player and contextual-rack features

The earlier 18-module rack remains available with multiple independent
instances, contextual controls, typed input connections and output selection.
Original Plaits VA/sine oscillators, modal percussion, bass drum and an externally
excited resonator are available, together with original stmlib digital LP/BP/HP
filters. These filters are the digital library algorithm, not a Ripples circuit
emulation. Spectral ADSR/AHDSFR, Spectral LP/HP/BP/notch, bypass, VCA and mixer remain.

Six starter patches: VA + digital filter, modal percussion, drum into resonator,
two differently tuned oscillators sharing a filter/envelope, sample into resonator,
and sample attack mixed alongside the resonator. Mixer inputs have
independent level controls. All loaded panels appear side by side, with blue
oscillators, green filters, dark envelopes/utilities and purple resonators. Module
type determines the knob/fader arrangement. Click sockets to patch; hover to see
connections. Focus module opens the enlarged panel and connection list.
Only reachable modules are constructed; dormant nodes keep editable parameter state.
The sample player has a gold panel, waveform, pitch/root note, start/end trim, level
and held looping. Load WAV or drop a file onto the panel. Both sample starters use
an original built-in click, so they work without external files.

App: `../../app/Builds/MacOSX/build/Release/Optisynth.app`, Apple Silicon,
version 0.0.6, bundle `com.yanwhite.optisynth.app`. Click Start audio, then play the
piano or Caps Lock typing keyboard (A/W/S/E/D/F..., Z/X octave). Silence stops the
output device. Pitch/octave labels are consistent across the piano and typing help.
No microphone, hardware MIDI or network listener is opened by this app.

Full `.optisynth` rack JSON preserves node IDs, separate instance parameters,
connections, outputs and gain. Rack v2 adds absolute sample paths/fingerprints;
WAV files remain external. v1 racks and old 0.0.2 instrument patches import with the same
active sound. Invalid connections, cycles, controls and malformed files are rejected
before replacing the graph. Required inputs may be incomplete only on dormant nodes;
reroute downstream destinations/output before removing an active dependency.

## Previous 0.0.5 verification

- 120 modular checks pass, including WAV decoding, sample pitch/rate/trim, looping,
  release/retrigger, independent playheads, deterministic sample/resonator starters,
  old DSP paths and no render allocation. All 120 also pass under ASan/UBSan.
  The 28 graph checks pass. Log: `../../build/sample-qualification.log`.
- Native 0.0.5 build and functional checks pass: all 18 panels fit without overlaps;
  controls remain independent; v1 rack/instrument and v2 sample recall preserve
  audio. Background loading, stale-result rejection, cancellation, dormant-file
  pruning and missing/changed-file preservation are exercised. Explicit reload
  cannot reuse a different instance's old cached audio.
- Nine native captures cover the six previous surfaces plus sample/resonator,
  parallel attack/mixer and the sample panel at 1120x800. Final text-only build,
  native recheck and visual review pass; see `../../design-qa.md`.
- No audio device is opened by automated verification. Finder-driven dragging and
  physical keyboard/audio audition remain user checks; handler paths are covered.
- At 0.0.3, 1,194 inherited core checks, four importer checks and one renderer timing
  check passed. Those unchanged compatibility-engine tests were not repeated here.
  Historical receipts: `../verification/rack_build_receipt.json`, `../verification/hardware_ui_build_receipt.json`.
- Existing converter coverage includes 44.1/48/96/192 kHz and plan crossfades. The
  benchmark in the modular log still measures the older two-source graph; it is
  not a sample/polyphony worst-case CPU claim. Load varied during this run.

Yan auditioned 0.0.2 and reported nice tones, then confirmed the Mutable examples
in 0.0.3 all seem to work (request 25). Yan approved the 0.0.4 UI direction with
“thats good” (request 26); this is visual feedback, not a separate audio test. No Live verification or new VST3
packaging/installation is claimed.

## Current direction and exact next work

Requests 17–19 (keep building; show available open-source Mutable modules) are
implemented in this rack milestone. Request 20's other-maker shortlist is recorded
in `../research/modules/OPEN_SOURCE_MODULE_MAKERS.md`, indexed by `../research/RESEARCH.md`: 4ms Ensemble Oscillator
and DaisySP are the recommended next sources to review. No code from those projects
has been adopted. All 42 user requests are retained verbatim in `USER_REQUESTS.md`.

Request 36 produced the 0.0.6 whole-Spectral stereo/state and shared-routing
increment above. It does not complete the subsequent sequencer/effect roadmap.

Requests 33–34 are expanded by request 38's sequencer research above. Shared Pattern
views/traversal options and separate process modules replace the earlier undetailed
shortlist. `../research/sequencers/SEQUENCERS_AND_ARPEGGIATORS.md` retains the local Optiseq lessons and
isolated clock/event prototype history. Playable sequencers/arps remain pending.

Request 35: `../research/modules/OPEN_SOURCE_DELAY_REVERB.md` records Clouds/Parasites, 4ms DLD,
Valley Plateau, CHOW Matrix, Airwindows and DaisySP candidates, published licences,
rate/stereo/feedback/tail requirements and outstanding adoption reviews. These
are research candidates, not new installed modules. Both research tracks inform
the whole-Spectral boundary and do not replace its implementation priority.

Request 30: sample player, immutable reachable assets, background loading,
contextual waveform panel and both sample/resonator starters are implemented.
Mechanism, format/resource bounds and acceptance are in `../architecture/SAMPLE_PLAYER.md`.
Build/source receipt: `../verification/sample_build_receipt.json`; user audition is pending.

Request 31: Pigments/Vital modulation assignment and a matrix overview are logged
as a UI option alongside module connections. The first shared matrix is now
implemented; source drag-to-knob assignment and knob depth indications remain
pending. User screenshot is retained locally; `../ui/CONTEXTUAL_UI.md` records both
the proposal and current behaviour. This did not cancel sample work.

Requests 28–29: VCV performance lessons and the right degree of modularity are
researched in `../research/platforms/VCV_ARCHITECTURE_LESSONS.md`. Current audio/control modules are
independent. In 0.0.6 parameters also accept shared matrix routes; note/gate context
remains shared, control modulation is block-rate, and event cables/polyphony/global scope/
feedback remain unimplemented. Recommend playable voice groups over an editable
modular graph. Define these interfaces before extending sample/polyphony ownership;
the sample slice retains the current shared Note context. The VCV research itself
introduced no runtime changes.

Request 27 asks whether to use direct module/cable manipulation, preset routing or
both. Recommendation: both on the same saved graph; prewired editable templates,
drag patching with optional cable visibility, and undoable edits. Proposed behaviour
and acceptance are in `../architecture/ARCHITECTURE.md`. No runtime change for this design question;
this interaction sequence remains queued alongside bounded polyphony/modulation work.

Request 25 is implemented: Matriarch-inspired native rack with familiar,
module-specific panels. References and behaviour are in `../ui/CONTEXTUAL_UI.md`.
The sample module extends that approved visual direction.

Requests 23–24: continue implementation across side-note requests. The current
Mutable rack was opened and auditioned successfully; this standing continuity
instruction remains in force after the UI milestone.

Request 22 adds a parallel Korg Wavestate repurposing project: a potential physical
controller or eventual hardware host for Optisynth. Both roles are recorded in
`../research/RESEARCH.md`; integration and hosting feasibility remain unverified. The current
module/sample/polyphony development order continues.

Request 32's complete engine, stereo/state boundary and first shared matrix are
implemented in 0.0.6. The older full Spectral editor's structural controls remain
to adapt. Sequencer next steps now follow request 38's research and UI architecture
above; preserve the effects review queue. Add broader
whole-engine editing and modulation indicators without splitting Spectral apart.
Use the Pigments/Vital suggestion for matrix/parameter assignment; keep one stable
routing representation. Broader bounded polyphony and voice/global boundaries,
more Mutable engines, granular playback, envelope/modulator options and undo/direct
cable editing remain queued. Further decomposition of Spectral is not the next step.
Hardware MIDI, audio-rate parameter modulation, event cables, agent API and a
modular VST3 wrapper are pending. This is a monophonic rack; all modules receive
the same note context. Hardware audio input is not implemented.

Source adapters target fixed 48 kHz / 24-sample blocks with device-rate conversion.
Sample decoding runs on one cancellable worker and only loads reachable assets.
Cached graph edits without full engines prepare on the UI thread; all reachable
Spectral graph preparation uses the worker. Scalar edits use resolved atomics.
Playback uses linear interpolation, with antialiasing refinement deferred. WAV
mono/stereo PCM/float is supported, stereo averaged to mono; 32 MiB per file/decoded
asset and 128 MiB of unique assets per plan. Crossfades keep more than one plan
alive, so this is not a process-wide memory cap. External sample files must remain
available for cold recall. Stereo WAV playback, portable collection and seamless
loops are pending; the new rack/Spectral output path already preserves stereo.

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
`../verification/source_snapshot_20261117.json`. The inherited compatibility engine and its plugin
source remain separate from the new modular runtime. The Spectral table-reading
extraction previously produced byte-identical Filth Bass, fn HiHat and Tortured Lead
2 comparisons. Original software/demo-noise boundaries remain unchanged.

Graph compilation rejects cycles and malformed/oversized structures, prunes dormant
nodes and prepares only active dependencies. Maximum 128 nodes, 1,024 connections,
64 ports/outputs. Construction/destruction are off the audio callback; scalar edits
and the tested render paths allocate nothing. JUCE/OS callback synchronization is
separate from these core guarantees.

Original Mutable DSP and dependencies are pinned with notices and exact source hashes
in `third_party/mutable/PROVENANCE.json`; reviews are in `../research/modules/MUTABLE_FIRST_SLICE.md` and
`../architecture/RACK_ROUTING.md`. Five classic-synth manuals/schematic archives (~38 MB) are stored
under ignored `docs/references/`; detailed Xpander/JP-8080 analysis remains pending.

## Backups and Git

Yan explicitly authorized regular commits and pushes on 17 November (request 21),
superseding the inherited no-commit rule for this project. Origin is
https://github.com/yanwhite73/optisynth.git. Push checked milestones and verify remote
branch equality. No changes or commits to the parent Optispectral project.

Independent source archives remain enabled via `tools/backup_source.py`. They now
include all four app headers, sample source/tests and our `.optisynth` patch files. Previous verified
archives: `../../../backups/optisynth/optisynth-source-20261117-163238.tar.gz` (136 files)
and `optisynth-source-20261117-155932.tar.gz` (98 files). Each has a checksum and an
internal file manifest. The current delivery's archive and Git receipt are recorded
in `ENGINEERING_LOG.md` after verification.

0.0.5 backup: `../../../backups/optisynth/optisynth-source-20261117-182753.tar.gz`
plus `.sha256`, 157 source files verified by fresh-directory restore. The new app
is open on Sample into resonator with audio initially off; existing user instance
preserved. All 83 parent Optispectral source hashes and 23 vendored Mutable files
remain unchanged. Receipt: `../verification/sample_build_receipt.json`.
