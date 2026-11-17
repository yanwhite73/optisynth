# Sequencer workflow comparison and Optisynth consequences

17 November 2026 · request 41 · [Visual atlas](README.md)

This comparison uses 54 inspected publisher images, plus collected primary manuals/
source: request 41's 39-image study and request 42's 15 OXI diagrams. It corrects
the earlier three-image sample. It is a design
study, not a hands-on usability test. Image IDs below link to illustrated source
pages; proposed Optisynth behaviour is explicitly our synthesis.

## A stable workspace with distinct editors

[Seq S01–S06](seq.md) is the strongest desktop precedent here: browser and inspector
stay recognisable while the centre becomes a step grid, piano roll, serial list,
parallel composition or graph. [PER|FORMER P02–P03](performer.md) demonstrates the
same economy at a much smaller scale with different note and curve editors.
[Bespoke B03–B04](bespoke.md) shows why Euclidean rings and variable-duration stages
need different representations even when both emit notes.

**Our proposal:** retain the approved contextual rack. Focus opens a stable frame:
module/clock/output header, track or object list, model-specific centre, scoped
inspector and optional emitted-event strip. This is shared navigation, not a single
universal grid. Compact panels retain each model's characteristic performance
controls; dense composition work belongs in focus. Exact dimensions remain to be
tested in our own wireframes.

## Editing and performance tasks

| Task | Image evidence | Optisynth interaction proposal | Acceptance question for the wireframe |
| --- | --- | --- | --- |
| Enter a bass phrase | [S01](seq.md), [B01](bespoke.md), [I02](impromptu.md) | Click to add/toggle; select a step to inspect pitch/duration; keyboard entry is optional | Can Yan enter four pitches without navigating away from the sequence? |
| Edit chords and held notes | [S02](seq.md), [B07](bespoke.md) | Piano roll preserves overlap and durations; inspector offers exact values | Does changing view preserve the phrase and current selection? |
| Change velocity or probability | [T01](stochas.md), [P02](performer.md) | Switch property overlay while retaining time/pitch coordinates; show numbers on selection | Can one tell a probability view from a change to the probability algorithm? |
| Shape modulation | [P03](performer.md), [B06](bespoke.md) | Typed curve/fader lane with range, units and interpolation | Can one see what destination the values affect and whether it is connected? |
| Change one lane's length | [K00–K01](monome.md), [P04–P05](performer.md) | Visible per-lane loop brackets, rate and phase; exact inspector fields | Can independent pitch and gate loops be understood without a shared misleading cursor? |
| Create a multi-pulse stage | [B04](bespoke.md) | Stage card shows pitch, dwell pulses, gate mode and current sub-pulse | Is a three-pulse hold visibly different from three retriggers? |
| Shape a rhythm generator | [B03](bespoke.md), [M02](mutable.md) | Euclidean rings use hits/length/rotation; rhythm map uses XY/density | Does each editor expose the process that actually owns its output? |
| Freeze an evolving phrase | [M01–M03](mutable.md) | Memory controls with loop/history preview and distinct capture action | Is freeze/reuse distinguishable from copying output into editable notes? |
| Explain a counter event | [C00–C02](monome.md) | Countdown strips plus source/target rule matrix and last-fired explanation | Can one identify which expiry changed another counter? |
| Use an independent arp | [B05](bespoke.md), [S06](seq.md) | Show held/latched input notes, traversal and generated order | Can silence be explained as no input, muted output or stopped clock? |
| Queue a phrase while editing another | [P06–P09](performer.md), [I01](impromptu.md) | Separate edit selection, playing phrase and pending launch; explicit launch boundary | Can a queued change be cancelled without modifying the playing phrase? |
| Arrange phrases | [S03–S05](seq.md), [B08](bespoke.md), [K02](monome.md) | Start with ordered scenes and parallel parts; advanced branch graph is separate | Does editing an occurrence clearly differ from changing its shared definition? |

These are proposed interactions and review questions, not implemented features or
claims that the upstream products use those exact mouse gestures.

## What the manuals establish beyond the still images

PER|FORMER documents step-plus-encoder editing, persistent multi-selection,
quick-access loop settings and separate playing/editing pattern identities. Pattern
changes can be grouped or synchronised to a boundary, and pending changes can be
cancelled. Performance controls use related mute/fill mechanisms. Snapshot provides
a temporary state that can be committed or reverted. These confirm that P04 and
P06–P09 represent different tasks, not merely alternate decorations.
[Primary manual](https://westlicht.github.io/performer/manual/).

Bespoke documents velocity adjustment on drum cells, row protection during
randomisation, and stage gate behaviours including repeat, once, hold and rest.
The note canvas provides recording/quantisation controls. Those gestures and
behaviours supplement B02/B04/B07; a still image alone cannot demonstrate them.
[Primary module documentation](https://www.bespokesynth.com/docs/index.html).

Impromptu documents editing versus playback positions and ATTACH following,
sequence versus song editing, and direct entry for some displayed values. Its
red/green position convention suggests adding shape/text in our design rather
than relying on colour alone. The panel images do not establish how discoverable
these operations feel in use.
[Primary manual](https://marcboule.github.io/ImpromptuModular/).

## Decisions sharpened by the visual evidence

### Scope is part of the editor

S01 visibly separates sequence, track and step inspectors. Adopt an explicit
breadcrumb such as `Pattern A / Bass / Steps 3–5`, inherited values with a reset
action, and mixed-value feedback for selections. A bulk action names its scope:
“Randomise selected steps”, not an ambiguous “Random”. Selection should survive
switching compatible views. Never make a playhead movement change the edit target
unless Follow is deliberately enabled.

### Four statuses must not share one highlight

Use distinct visual channels for selected item, sounding item, queued launch and
modified/uncommitted content. Proposed examples: selection outline, moving playback
marker, labelled pending badge with boundary countdown, and an unsaved marker.
Colours reinforce these roles but are not their only encoding. P07/P08 are the
closest collected pair showing an arrangement changing into playback; we still
need our own queued/cancelled/error states in wireframes.

### Performance gets its own surface

P09 and I04 privilege actions that work while music runs. Provide an Edit/Perform
choice within a module or conductor, with the clock and destination still visible.
Perform exposes mute, fill, variation, capture and queued pattern launch. It must
not reinterpret a note-edit click as a launch command. Momentary fills and saved
pattern edits have different persistence. The initial module need not implement
every performance action at once.

### Each generative model needs an explanation

A generic piano roll can show captured output, but cannot edit remembered random
history, counter links or map position. M03 suggests displaying both distribution
and realised history; C02 suggests a readable rule beside its graph/matrix. Keep
the authoritative process controls visible. Label output previews as previews;
capture produces a new editable Pattern while preserving the original generator.
The arp likewise needs visible input ownership, not just an attractive output grid.

### Desktop adaptation should reduce hidden gestures

The Monome diagrams explicitly annotate functions that bare hardware pads cannot
label. Keep their musical organisation, but show names, parameter lanes and a
selected-item inspector on screen. Expert shortcuts can supplement visible actions.
Keyboard navigation and numeric entry must coexist with the existing Caps Lock
performance keyboard: editing text/numbers must not trigger notes, and releasing
focus must not leave held notes behind. This is an Optisynth requirement to verify,
not a tested characteristic of the reference applications.

### Defaults and absent output need a useful explanation

B04's published empty stage state contains zero pulses; B05 has no input-note
display. Our initial templates should produce an understandable musical result.
Explain an empty pattern, no held arp notes, a zero-duration stage, an unconnected
destination, a stopped external clock and muted output in context. Do not label
every silent source as an error: intentional rests and holds remain valid.

## Remaining evidence and next design exercise

Request 42 adds the [OXI illustrated study](oxi.md) and
[model assessment](../../research/sequencers/OXI_ONE_REFERENCE.md). Include these
additional cases in the proposed wireframes:

| Task | OXI evidence | Our design consequence |
| --- | --- | --- |
| Change a chord's quality or inversion | OX04–05 | Keep harmonic intent editable and show the resulting voiced notes |
| Advance a phrase only when a trigger fires | OX08 | Separate rhythm and note cursors; show the advance source and policy |
| Lengthen a stage without adding stages | OX09–10 | Show both stage count and total musical duration |
| Follow another phrase harmonically | OX14 | Expose the leader relationship and effective chord/scale |
| Locate a mute applied in performance view | OX12, official performance chapter | Persistent performance-state badge in the editor |
| Plan an arrangement without starting it | OX11 | Separate opening, editing and arming the arrangement |

OXI's shared physical grid changes its axes between models. Reusing the desktop
frame must not conceal whether a column means time, pitch probability, a stage or
an arrangement slot. Stochastic output gets its own history display; its control
grid is not a piano-roll timeline. These are additions to the research proposal,
not implemented modes or a commitment to copy OXI's complete feature set.

The collection now covers all main proposed editors. It does not establish pointer
feel, touch targets, keyboard accessibility, minimum window size, live timing,
animation quality or CPU cost. Most references are single published states rather
than a complete operation recorded from beginning to end. Marbles/Grids are panel
diagrams; the distribution figure is explanatory material. No app was installed,
built or connected to audio/MIDI for this study.

Next, compare static Optisynth Pattern, Arp, Stage and Memory wireframes against
the tasks above, including selected/playing/queued/empty states. Resolve the typed
lane/event/port contract and complete any chosen upstream adoption review before
device implementation. If a particular gesture or state cannot be resolved from
the collected manual and images, obtain targeted evidence for that question;
do not infer successful interaction from a screenshot count.
