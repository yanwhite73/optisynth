# Optisynth — verbatim user inputs

Captured 17 November 2026, in conversation order. Wording and spelling are
preserved. Later additions steer the same project unless explicitly cancelled.
Implementation status lives in `STATUS.md`; research lives in `../research/RESEARCH.md`.

## 1 — Multi-engine instrument and agentic composition

```text
how feasible is it to branch this into a multi-engine synth that can also use samples, granular, modes emulating JP8080, Oberheim xpander, mutable instruments modules etc?
Having my own modular environment would make composing with agentic tools much easier
```

## 2 — Physical modelling and hybrid percussion

```text
I would also add to that a virtual modelling mode mainly for percussion and drums, perhaps using samples alongside or with resonators
```

## 3 — Shared, routable components

```text
Having parts of each synth engine modular in a way that they can share elements like envellopes or fx, and routable would be amazing
```

## 4 — Separate project and modular Spectral rebuild

```text
If you start work on this, clone the repo and we would start on a new project and rebuild spectral so some if its acrhitecture is modular
```

## 5 — Initial working folder

```text
for now im calling it /Users/yanwhite/Documents/Music\ docs/Code/multisynth
```

## 6 — Manuals and schematics research

```text
part of this should also be a research project which would involve finding schematics and manuals for aforementioned classic analog synths
```

## 7 — Performance and loading only used modules

```text
the architecture should be performance-friendly, escpeically if made into a vst, so probably only load modules as they are used
```

## 8 — Mutable source and existing ports

```text
mutable source should be available open-source and there are plenty of clones and vcv rack devices to look at for pointers
```

## 9 — Project-local research documentation

```text
collect reference and research in docs in the folder. Documentation is necessary for a successful project
```

## 10 — Verbatim input capture

```text
including as always verbatim my inputs
```

## 11 — Project renamed and GitHub repository supplied

```text
renamed to optisynth
https://github.com/yanwhite73/optisynth.git
```

## 12 — Mutable first; interchangeable envelopes/filters; shared environment

```text
start with mutable modules, and have multiple options for envelopes and filters, starting with the ones from spectral. I am thinking along modular lines, but an environment, standalone synth and possibly vst which can load all these different modules and engines
```

## 13 — Contextual module interfaces

```text
UI matters a lot here as well. Use vcv modules as a guide or the mutable pages / original modules. Loading a different oscillator or resonator should load contextual ui for that module
```

## 14 — Circuit modelling question

```text
would you need to model circuits for the mutable code for it to sound good?
```

## 15 — Combined envelope routes sound rough

```text
sounds good so far, nice tones, some aliasing when both checkboxes selected
```

## 16 — Defer sound-quality and performance refinement

```text
maybe just log it, lets move quickly and refine performance and sound wquality later
```

## 17 — Keep building

```text
Ok dont stop
```

## 18 — Prioritize visible Mutable modules

```text
I'd like to see the mutable modules soon, osc, filter, etc
```

## 19 — Use available open-source implementations

```text
whichever ones have open source code
```

## 20 — Other respected makers with open digital-module code

```text
which other well respected modular builders release open source code for digital modules that we could use?
```

## 21 — Log research and back up through Git regularly

```text
great log all this and make sure to commit and push regularly
```

This explicitly supersedes the inherited no-commit/no-push restriction for
Optisynth. Commit and push coherent, checked milestones to the supplied origin;
retain local source archives as an independent backup.

## 22 — Parallel Wavestate repurposing project

```text
add another note, a parallel project to repurpose a korg wavestate might become a useful controller or even host
```

## 23 — Continue the active build across side notes

```text
why did you stop?
```

Correction: documentation requests and research questions steer ongoing work;
they do not cancel the standing instruction to continue implementation.

## 24 — Inspect the current Mutable implementation

```text
Let me see the mutable work so far
```

## 25 — Listening confirmation and familiar contextual module UI

```text
yes all seem to be working ok. I'd like a moog matriarch style ui where loading a module loads familiar ui for that module
```

User audition confirms the current Mutable examples seem to work. Prioritize the
Matriarch-inspired contextual rack UI next; sample/excitation work remains queued.

## 26 — Contextual UI feedback

```text
thats good
```

Positive feedback on the opened 0.0.4 contextual rack; retain this UI direction.

## 27 — Direct patching versus preset routing

```text
drag and drop / remove / patch cables? or a more preset-based routing architecture? or both?
```

Design question: recommend both as interfaces to the existing routing graph.
Editable, prewired instrument templates plus direct module/cable manipulation.
This is a recommendation, not a claim that drag cables or undo are implemented.

## 28 — VCV performance lessons

```text
what can we learn from vcv about performance etc?
```

Inspect primary VCV engine/developer evidence and compare it with Optisynth.
Record practical architecture lessons, limits and measurements before optimization.

## 29 — How modular are the modules, and how modular should we make them?

```text
and also are modules actually modular as they would be in vcv, and do we want that for the purposes of this synth / environment?
```

Distinguish current DSP/typed graph modularity from VCV's voltage-patching model.
Assess the shared note context, modulation ports, polyphony and voice/global scope;
recommend a modular core with playable voice groups without claiming these exist.

## 30 — Continue implementation

```text
ok continue
```

Continue with the next playable milestone: immutable sample assets, background
loading, a contextual sample player and sample/resonator starter routes. Preserve
the modular boundaries described in the VCV research; keep broader polyphony and
event routing as subsequent work.

## 31 — Consider Pigments/Vital modulation UI

```text
[Image #1] another option to consider is the mod matrix style of pigments or vital
```

Attached screenshot: Pigments harmonic engine, filter panels, coloured modulation
source strip and envelope/macros section (Screenshot 2026-11-17 at 18.11.23.png).
Consider source selection or dragging onto parameters, visible modulation depths,
and a matrix overview alongside module audio/control connections. This steers
the modulation design; it does not cancel the sample-player milestone in progress.

## 32 — Complete Spectral engine with cross-module matrix routing

```text
ok i guess next would be including spectral not as individual modules but with a routing matrix that can reach other modules
```

Next priority: integrate Spectral as a complete engine retaining its internal
architecture. Expose it to the environment's shared routing matrix so it can
interact with other modules. Inspect audio and modulation boundaries and existing
parameter/state handling first. Existing extracted envelope/filter options remain
available; further decomposition of Spectral is not the requested next step.

## 33 — Independent sequencer/arpeggiator and VCV/Optiseq lessons

```text
we will need a sequencer too, as well as independent arp, though spectral has one.
What can we learn from vcv sequencers and optiseq?
```

Add reusable sequencer and arpeggiator modules to the environment's requirements,
separate from Spectral's retained internal arp. Research primary VCV sequencer
manuals/source and the local Optiseq implementation; document useful interaction,
musical, timing and routing lessons. This research steers the shared event/clock
architecture while preserving the whole-Spectral integration priority.

## 34 — Different sequencer models

```text
having different seuqncer models would be great, thats one of the strengths of modular
```

Require multiple independently loadable sequencer models, each with contextual
controls and characteristic behaviour, sharing clock/note/modulation interfaces.
Allow several models to run and interact in one patch. Preserve the distinction
between changing a sequencer's algorithm and displaying the same pattern differently.

## 35 — Open-source delay and reverb modules

```text
what open source delay and reverb modules are there? these would be good for a modular system
```

Research original delay/reverb DSP and modular implementations, their published
licences and likely adaptation work. Record a practical shortlist with source links;
this adds effects to the module roadmap without silently replacing the complete
Spectral engine or independent sequencer/arpeggiator requirements.

## 36 — Start building

```text
ok start building
```

Proceed with the next implementation milestone: integrate the complete Spectral
engine into the standalone rack with stereo/state preservation and shared routing
controls. Retain the sequencer and open-source effects requirements in the plan.
Build and qualify a usable standalone increment, then commit, push and back it up.

## 37 — Standalone 0.0.6 audition feedback

```text
all seems to work ok
```

Record the user's successful general audition of the delivered standalone. This
does not establish exhaustive preset, performance or Live/VST3 verification.
Continue the accepted next milestone: shared sample timing and note-event rules,
then independent sequencer and arp modules. Preserve the working 0.0.6 app while
qualifying the timing foundation offline.

## 38 — Research sequencer models and UI architecture before further building

```text
collect proper open source sequencer research before building anything, categorise models and methods, and then see if you can synthesise that into modes, determining what UI can be shared and where it makes more sense to either build a configurable multi-mode sequencer, or standalone modules
```

Pause implementation. Collect substantive primary manuals, source and licence
evidence; categorise stored-data models, generation and traversal methods,
interaction and performance needs. Synthesize a proposed set of modes, shared UI
components and separate modules, explaining the tradeoffs. Save the research and
verbatim request. The existing timing prototype remains experimental context,
not a constraint on the research or authorization to build the chosen design now.

## 39 — Confirm visual/interface research coverage

```text
are you collecting ui research as well? screenshots and interface?
```

UI evidence is part of the sequencer research. Distinguish the three collected and
viewed reference images (Stochas screenshot, Kria navigation diagram, Meadowphysics
counter diagram) from broader manual/source-derived interaction findings. Expand
visual coverage of the remaining representative interfaces before wireframes or
implementation; do not imply a complete screenshot/workflow survey already exists.

## 40 — Organise documentation into folders

```text
docs needs folders for sorting
```

Sort project memory, architecture, UI, research, verification evidence and inherited
Optispectral documents into clear folders. Add a documentation index and update
current links, agent entry points and helper paths. Preserve original request text
and immutable provenance/verification records; do not rebuild the synth.

## 41 — Insufficient sequencer screenshots and UX evidence

```text
in sequencer research you have not added sufficient ui screenshots, so when you come to build the device you probably wont have a good paradigm for the ux
```

Correction accepted: the three-image sample does not sufficiently ground the UX
proposal. Collect and inspect a substantial set of actual sequencer interfaces and
editing/performance states; document workflows, shared interaction patterns and
model-specific controls alongside the images. Revise the proposal from that visual
evidence before any device implementation. Do the collection now, not just log it
as another future task. Preserve the organised documentation folders.

Delivered: 39 inspected images, eight illustrated source pages and a cross-source
workflow comparison in `../ui/sequencers/`. Revised the proposal around the visual
evidence. Static/manual study is complete for this collection; hands-on usability
and our own wireframe evaluation remain separate next steps. No device was built.

## 42 — Include the OX sequencer

```text
you should include the ox sequencer
```

Include the requested sequencer in the research and visual/workflow comparison.
Identify the intended product from primary sources; OXI ONE is the initial working
interpretation. Capture actual interface references and model-specific lessons,
and distinguish a commercial UX reference from verified reusable open-source code.

Delivered under that working identification: OXI ONE MKII model/source assessment,
15 inspected and illustrated manual diagrams, 13 chapter snapshots and revisions
to the shared editor/mode proposal. See `../research/sequencers/OXI_ONE_REFERENCE.md`
and `../ui/sequencers/oxi.md`. Firmware/manual edition and unresolved source/table
limitations are explicit. No device implementation.

## 43 — Start building sequencers

```text
start building some sequencers
```

19 November 2026: resume implementation using the collected model and UI research.
Deliver playable, independently routable sequencer modules with contextual editors,
shared transport, saved native state and qualified note-release behaviour. Choose
a coherent first set, build the standalone app and verify it offline/native before
handoff. This supersedes the earlier research-only pause; keep later specialist
models and the broader polyphony/VST3 roadmap explicit rather than implying them
complete. Preserve regular commits/pushes, backups and the no-Live-driving rule.

## 44 — Project review; ambient Berlin School and lush analog swell goals

```text
review this project.
Some additional goals to be captured:
modular sequencing styles Im aiming at include robert rich and steve roach ambient psychedelic berlin school, organic detailed evolving sounds
also some of the lush analog synth swells that they might get using oberheim or Prophet style synths
```

19 November 2026 (Claude session, concurrent with the Codex session building
request 43). Two parts. (1) Review the project: architecture, code, docs and
process against the stated goals. (2) Record new aesthetic goals as design
targets: Robert Rich / Steve Roach-style ambient, psychedelic Berlin School
sequencing; organic, detailed, slowly evolving sounds; lush Oberheim / Prophet-
style analog pads and swells. These steer sequencer, modulation, voice and
effects priorities; they do not cancel request 43's sequencer build.

## 45 — Sequencer delivery appears to be an older app

```text
I dont see it. That looks like 0.0.4
```

19 November 2026: investigate the running app and bundle location rather than
assuming the user reached the newly built binary. Verify the delivery version,
resolve the launch path and make the sequencer starter directly visible. Preserve
the user's current app/session state; no Live interaction or new DSP work needed.

Resolved handoff: verified bundle version 0.0.7 and the qualified executable hash,
then opened the exact build with `--sequencer-demo`. Confirmed its running process
path. The earlier 0.0.4 appearance was not reproduced or attributed to a specific
old copy. The next feedback confirms the user reached the new sequencer.

## 46 — First sequencer pass feels underwhelming

```text
mm ok its a start, a bit underwhelming
```

19 November 2026: technical qualification and a playable first slice have not yet
delivered the musical depth or interaction quality the user expects. Treat this as
an unmet product-quality goal, not an endorsement of the minimal editors. The
next proposed pass should combine direct sequence editing with independent lane
lengths/directions, probability/ratchets and evolving patterns, plus convincing
ambient/Berlin School starter patches using delay/reverb. Keep the independent
arp and voice-group roadmap visible. These are proposed priorities, not features
implemented by this feedback entry.

## 47 — Availability of schematics, plugins and open-source versions

```text
first thing to decide is how much schematic info is available on how those synths are built? any plugin versions? open source versions?
```

19 November 2026 (Claude session; follows request 44's goals). "Those synths"
are the reference instruments named for the Rich/Roach and Oberheim/Prophet
goals: Oberheim SEM, OB-X/OB-Xa/OB-8, Xpander/Matrix-12 and Prophet-5 (plus the
Prophet-6 Rich cites). Survey what circuit documentation (service manuals,
schematics, chip datasheets), commercial plugin emulations and open-source
implementations exist, with direct sources and licence notes, as the first
decision input for the analog pad voice. Research only; no code or downloads
are implied as adopted.

## 48 — Excessive time spent documenting feedback

```text
7m just to document what I said??
```

Keep feedback capture brief; do not turn ordinary discussion into separate Git
housekeeping deliveries.

## 49 — Mature sequencer implementation; bundle commits

```text
bundle commits. You are by far too pedantic while missing the obvious tasks.
But the sequencers, while basic, function. Continue based on the full sequencer module research, and make at least one of them a mature and sophisticated version - maybe just implement an open source design if one is available that meets those criteria
```

Proceed with a substantial playable implementation grounded in the full research.
Choose a mature reference, assess practical source reuse and deliver one complete
sequencer/editor improvement. Bundle implementation, verification, docs and backup
into the delivery commit; no separate feedback-only commits. Retain module
routing, preset compatibility, host boundaries and user-performed listening.

## 50 — Arturia owned; plugin scan; commercial intent

```text
I have arturia, you could just scaan my plugins folder. Ultimately commercial btu we could learn from other projects
```

19 November 2026 (Claude session; recorded concurrently with requests 48–49 to
the Codex session). Answers request 47's two questions. (1) Yan
owns Arturia instruments; scan the local plugin folders read-only to establish
which emulations are installed (listing and bundle metadata only; no plugin is
loaded, run or modified). (2) Optisynth is ultimately intended as a commercial
product. Other projects (including GPL ones) may be studied to learn techniques
and behaviour, but adopted code must be licence-compatible with closed
commercial distribution. Record the resulting licence policy.

## 51 — Research the actual architecture of the classic pad synths

```text
Forget selling for now. I want to see if results are useful first.
You could do an inventory of parameter names and values, but that wont necessarily give the character of the sound.
If you can decompile and learn from that without copying then ok.
I askewd about schematics and research on the actual architecture of those classic pad synths.
```

Commercial constraints are deferred. Correction: the request was for circuit-level
architecture research (schematics, service manuals), not plugin inventories.

Request 49 implementation: Pattern / Motion and two FX starters are built; 221
modular checks pass normally and under ASan/UBSan, with exact stereo preset recall.
Native UI execution/visual checks remain blocked by desktop startup failures.
The delivery commit bundles source, tests, presets and docs. See STATUS for the
exact pending qualification; it is not closed merely because code compiles.

## 52 — Resume Motion qualification and delivery

```text
codex resuem
continue
```

19 November 2026: resume request 49 at native editor verification, then complete
the source push. Keep the existing bundled implementation commit; preserve the
concurrent classic-synth research. The supplied workspace Git discipline requires
checking the actual remote head and reporting any remaining delivery blocker.

## 53 — Consistent build delivery and agent discipline

```text
why is there no consistency in where you put the builds? I am seeing increasingly chaotic and sloppy practices from codex
```

```text
DISCIPLINE
```

```text
are you logging my feedback?
```

19 November 2026: correction accepted. The fallback Motion build was handed over
from `build/motion-native` while the established release path still contained
0.0.7. This made the user track changing paths and versions. Fix the delivery
workflow, not just the wording: one stable user-facing app location, consistent
build/promotion scripts, explicit version and qualification state, and an archived
previous app. Capture feedback verbatim immediately and bundle the correction,
verification and records together. Recording this request does not complete it.

## 54 — Research has not produced a mature musical sequencer

```text
again, this is disappointing work. I have carefully shepherded copious research only to get very basic and uninspiring sequencer implmentation.
```

19 November 2026: request 49 remains unmet at the product level. More step controls
and passing DSP checks have not delivered the researched musical workflows or
interface depth. Compare the actual editor with the saved research and implement
an integrated musical workflow, with visible interaction and playback evidence.
Keep the established delivery path. Do not describe another incremental control
addition as a mature sequencer or treat this feedback entry as completion.

```text
thats vague bullshit.
I asked for a sophisticated sequencer, based on the types of products found in modular devices
```

Correction to the response: a vague promise to improve musical workflow is not a
concrete interpretation. Target a complete modular performance sequencer using
PER|FORMER/Foundry-class behaviours: independent tracks and outputs, pattern banks,
quantized launch and phrase chains, separate edit/play state, ties/slides/ratchets/
conditions and live performance controls. Keep the researched specialist models
distinct. This is a proposed implementation contract, not a claim of completion.

Delivered response: 0.0.9 implements the four-track bank/chain instrument above,
with two routed four-part starters and an actual contextual native editor.
251 engine checks pass normally and under sanitizers; complete state/audio recall,
native component interactions and normal/minimum-size visual review pass.
Desktop-window startup is still unverified in this execution context. See
`../architecture/PERFORMANCE_SEQUENCER.md`, STATUS and the build receipt. The
feature delivery does not substitute for Yan's assessment of the musical result.

## 56 — Modular mix-and-match of the classic circuits; performance and latency

First recorded as 52 in commit 4126d4d while another session also used 52–55; renumbered so every request number is unique.

```text
is there anything thet awould prevent us from taking a modular approach to these circuits, allowing mix and match? performance? latency?
```

## 55 — Scrolling changes faders; result still disappointing; commit discipline

```text
scrolling causes faders to move.
Overall its not an exciting result. Please COMMIT YOUR WORK YOU SLOPPY BASTARD
```

19 November 2026: disable wheel-based parameter changes throughout the standalone
so navigating the rack does not edit its sound. Preserve deliberate dragging and
numeric entry. The product-level dissatisfaction remains open: the four-track
feature delivery has not met the user's musical expectations. Do not mark that
resolved by this interaction fix. Prior implementation was committed as f92326f;
current main 4126d4d also includes the concurrent research. Bundle this correction,
verification and feedback record in a scoped commit and retry the authorised push.

## 57 — What to build first

```text
ok what do you suggest building first?
```

## 58 — Build the polyphonic analog pad voice

```text
ok
```

Approves the request 57 proposal; this Claude session builds it.

## 59 — Bring much more of the collected sequencer references into the product

```text
I want to see much more of this..
/Users/yanwhite/Documents/Music\ docs/Code/optisynth/docs/references/ui
```

19 November 2026: use the supplied local visual/manual evidence as the direct
source for the next sequencer pass. Preserve the scroll correction and stable
delivery path. Current interpretation is both richer musical interaction and
recognisable contextual interfaces, rather than another list of numeric fields.
The separate-device clarification below establishes the direction. Concurrent pad-voice WIP
commits and the generated project file belong to the other session and must be
preserved; inspect their publication/build state before combining deliveries.

```text
they can be separate sequencers
```

Clarification: distinct devices are appropriate. Start with a probabilistic
pitch/time grid informed by Stochas and a four-ring rhythm module informed by
Bespoke's Euclidean view. Each gets its own editor and routable outputs; retain
Performance and the other existing sequencers. Use the references for musical
representation and gestures, with original implementation and artwork.

Delivered response: 0.0.12 adds these two separate modules and routed starters,
with actual grid/ring editors, source-to-interface mapping and five inspected
native captures. 307 engine checks, full preset/audio recall and native gesture/
scroll checks pass. Stable app path retained; the previous 0.0.11 app is archived.
See STATUS and `../ui/REFERENCE_SEQUENCERS_REVIEW.md`. User musical acceptance and
full desktop/audio interaction remain open; no whole-source-product parity claim.

## 60 — Sequencer disappears when changing sounds

```text
what happened to the sequencer? I changed sounds and it disappeared
```

## 61 — Listening feedback on the current sound and sequencers

```text
the current sound thats playing is actually starting to sound like music, and the sequencer options are a good start
```

```text
ok done
```

Saved listening reference: `patches/lush ambient space.optisynth` (local), Performance sequencer and analog pad at 118 BPM.

## 62 — Standalone app icon

```text
use one of my /Users/yanwhite/Documents/Art/2025/code\ based/optimystery/luckydip/screenshots/v2 images for the desktop icon, or a detail so it can be seen, and an O (serif)
```

## 63 — Icon visibility and stable app location

```text
hasnt updated. Dont be moving the build around
```

## 64 — Continue the pad-voice "not done yet" list; learn from Arturia's engineering

```text
contonue with not done yet. Dont assume arturia is perfect either, but they may have done some clever stuff with latency and performance, quality levels, threading etc
```

Claude session. Remaining items from the 0.0.14 handoff: glide, unison, Xpander
pole-mixing modes, Prophet poly-mod into oscillator pitch/PW, more than 8 voices,
tuning tables, and comparison with references. Research how Arturia (and others)
handle quality levels, threading, latency and CPU, as evidence rather than as a
standard to copy.

## 65 — Learn from the Wavestate executable captured by Optistate

```text
one thing that might be useful, although a different style of synth, is an exe version of wavestate that was copied from acessing the hardware in optistate. It might tell us something about architecture or efficient coding styles
```

Study only: inspect the captured binary statically for architecture and
efficiency lessons (threading, block sizes, voice handling, SIMD). Do not run it
or copy its code.

## 66 — Approve loading Arturia plugins for reference comparison

```text
yes
```

Answers the 0.0.15 handoff question: load the installed Arturia instruments in a
test tool to compare against Optisynth. Workspace rule: read their documentation
and record what they control, write and open before the first load; offline
rendering only, no audio/MIDI device, no Live.

## 67 — Were the comparisons started from init patches?

```text
are you starting with init patches?
```

Correction: they were not. Arturia AUs load an "Untitled" state and expose only generic program-change presets, so init presets are converted from their preset files (tools/reference_render/arturia_preset.py) and applied before test overrides.

## 68 — SEM V2 plays in its own app, but not from the computer keyboard

```text
I can confirm it does, but doesnt respond via CAPS keyboard. I pressed its keyboard keys
```

SEM V2 is activated and sounds from its on-screen keys; it ignores computer-keyboard MIDI. Points to MIDI input (e.g. channel) rather than licensing.

## 69 — Use MIDI channel ALL

```text
Just use ALL ?
```

## 70 — Read init patches instead of assuming and testing

```text
dont make blanket assumptions and then run tests on them. Init should give a good idea of what the routing needs for each synth
```

Correction: stop applying guessed overrides and probing them by trial. Read each init preset as the description of that synth's routing and voice settings, and derive the test configuration from it.

## 71 — Scalable modular view, a synth-panel view, and a richer preset browser

```text
Improve the UI, allow scaling of modular view, an alternate 'synth panel' view dependent on patch or preference.
Improve and enhance preset view, with tags, categories, favourites
```

Claude session, 20 November 2026. Four parts:
- General UI improvement of the standalone app.
- Zoom (scaling) for the modular rack view.
- A second "synth panel" view: a fixed, hardware-style front panel of the
  patch's playing controls, with no sockets. Which view opens is chosen by the
  patch, or overridden by a user preference.
- A preset browser to replace the flat starter menu: categories, tags,
  favourites, search, and saving into a library.

## 72 — 0.0.16 review: uniform scaling is wrong; rows; missing user preset; arrows; keyboard

```text
This is embrassingly bad. You dont just scale everything uniformly. Use a frontend framework properly and keep text readable, decreasing space between controls.
allow rows for modules.
there is a user preset, which I saved recently.
the arrows dont work
the keyboard seems to be not working now
```

Correction to request 71's rack zoom: a transform shrank text with everything
else. Zooming out must tighten spacing and control size while text stays
readable, using real layout (JUCE FlexBox/Grid) rather than a transform. Modules
need rows. Defects reported in hands-on use: a recently saved user preset is not
in the browser; the previous/next arrows do nothing; the keyboard no longer
plays.

## 73 — Move the saved preset into the user preset folder

```text
move it to  ~/Music/Optisynth/Presets/User
```

Done 20 November: `patches/lush ambient space.optisynth` (untracked) moved to
`~/Music/Optisynth/Presets/User/` after a byte-identical copy check. User presets
now live in `Presets/User`; the browser scans `~/Music/Optisynth/Presets`.

## 74 — Use the JUCE UI toolkit

```text
isnt there a Juce UI toolkit?
```

Yes: JUCE itself (Grid, FlexBox, LookAndFeel, TableListBox). Request 72's rework
uses it; options surveyed in `docs/research/platforms/JUCE_UI_OPTIONS.md`.

## 75 — Use existing frameworks and open source without being reminded

```text
I have to continually prod and poke to remind you agents to use existing frameworks and open source code
```

Standing rule recorded (engineering log request 72; session memory): survey JUCE
built-ins and licence-checked open source before building UI or infrastructure.

## 76 — Obvious expectations should not need explaining

```text
Why do I have to explain everything that should be obvious?
```

Recorded with request 72's lessons: test as a player (keyboard sounds, arrows,
readable text, presets where the user saves them), not only bounds and recall.

## 77 — Install the pending 0.0.17 update at the existing location

```text
I DONT SEE ANY CHANGE. HAVE YOU MOVED THE BUILD AGAIN?
```

Yan supplied the prior delivery conversation, including its cancelled watcher
and the report that 0.0.16 was still installed.

```text
It is not updating
```

Delivered at the existing pinned path. See STATUS for independent readback.

## 78 — Quit and reopen to use updates

```text
can we prevent annoyances like this happening in future? Every time the synth is updated I expect to be able to just quit and reopen
```
