# Sequencer visual atlas

17 November 2026 · requests 41–42 · [UI index](../README.md) · [Workflow comparison](WORKFLOWS.md)

**54 actual reference images, all saved and visually inspected:** 20 published
software screenshots/panel captures and 34 manual diagrams or screen examples.
Request 41 expanded the initial sample to 39 images; request 42 adds 15 OXI ONE
MKII diagrams, taking coverage to nine source groups. Each illustrated page separates visible observations from
our proposed Optisynth adaptation. These are publisher images, not our own live
captures or generated mockups.

## Browse by interface family

| Source | Images | Editing/performance coverage | Illustrated analysis |
| --- | ---: | --- | --- |
| PER\|FORMER | 10 | Navigation, note/curve editing, quick access, settings, pattern launch, song before/during playback, performance and routing | [Open screens](performer.md) |
| Bespoke | 8 | Notes, drums, Euclidean rings, stages, arp, control sequence, piano roll and song list | [Open modules](bespoke.md) |
| Seq | 6 | Step inspector, note events, serial/parallel composition, branching and arp | [Open editors](seq.md) |
| Impromptu | 4 | Foundry, PhraseSeq16, GateSeq64 and BigButtonSeq2 | [Open VCV panels](impromptu.md) |
| Mutable | 3 | Marbles, Grids and distribution explanation | [Open panels/diagram](mutable.md) |
| Monome | 6 | Kria navigation/parameters/meta-pattern; Meadowphysics counters/reset/rules | [Open interaction diagrams](monome.md) |
| Stochas | 1 | Probability labels, dependency chains and separate layer/pattern/view controls | [Open screenshot](stochas.md) |
| Orca | 1 | Spatial program and output/status display | [Open screenshot](orca.md) |
| OXI ONE MKII | 15 | Mono/Poly/Multitrack/Chord, palette, Stochastic, Matriceal, Domino, Saga, arrangement, performance, modulation and harmony | [Open OXI diagrams](oxi.md) |

The [workflow comparison](WORKFLOWS.md) turns this collection into concrete
editing tasks, shared components, specialist editors and acceptance questions.
The [architecture proposal](../SEQUENCER_UI_ARCHITECTURE.md) incorporates those
findings; no device implementation accompanies this research.

## Representative comparisons

### One grid, distinct musical models — OXI ONE

[![OX08: Domino trigger and note layers](../../references/ui/oxi_20261117/OX08.webp)](../../references/ui/oxi_20261117/OX08.webp)

OX08 distinguishes the rhythm that advances a phrase from its note sequence.
Compare the [full OXI set](oxi.md), including chord palette, duration-based stages
and arrangement/performance states. The [model assessment](../../research/sequencers/OXI_ONE_REFERENCE.md)
records how these change the Optisynth proposal. OXI is included as a commercial
UX reference; reusable engine source has not been established.

### Shared frame with explicit editing scope — Seq

[![S01: Step sequence and inspectors](../../references/ui/sequencers_20261117/S01-step-sequence-and-inspectors.png)](../../references/ui/sequencers_20261117/S01-step-sequence-and-inspectors.png)

S01 shows separate Sequence, Track and Step inspectors beside the grid. Compare
[S02–S06](seq.md): the central editor changes substantially without losing the
workspace structure. Proposed lesson: share navigation, selection and scoped
inspection while preserving the model's own editor.

### Stage duration needs its own representation — Bespoke

[![B04: Variable duration stages](../../references/ui/sequencers_20261117/B04-variable-duration-stages.png)](../../references/ui/sequencers_20261117/B04-variable-duration-stages.png)

B04 puts pulse count and gate behaviour beside every stage. Compare the
[Euclidean rings and note/control editors](bespoke.md). Proposed lesson: show
stage dwell explicitly; a conventional fixed-width note grid is not sufficient.

### Arrangement before and during playback — PER|FORMER

[![P07: Song chain example](../../references/ui/sequencers_20261117/P07-song-chain-example.png)](../../references/ui/sequencers_20261117/P07-song-chain-example.png)

[![P08: Song playback](../../references/ui/sequencers_20261117/P08-song-playback.png)](../../references/ui/sequencers_20261117/P08-song-playback.png)

P07/P08 show a consistent song table with changed playback feedback. The
[full set](performer.md) also includes pattern selection and a separate
performance page. Proposed lesson: distinguish edit selection, current playback
and pending launch without moving the user's workspace.

### Stateful generation needs process controls — Mutable

[![M01: Marbles control hierarchy](../../references/ui/sequencers_20261117/M01-marbles-control-hierarchy.png)](../../references/ui/sequencers_20261117/M01-marbles-control-hierarchy.png)

M01 places rhythm/value sections around memory controls. Compare the
[Grids panel and distribution diagram](mutable.md). Proposed lesson: retain
the process controls and supplement them with history/output inspection.

### Rules need visible causes — Meadowphysics

[![C02: Meadowphysics rule selection](../../references/ui/sequencers_20261117/C02-meadowphysics-rule-selection.png)](../../references/ui/sequencers_20261117/C02-meadowphysics-rule-selection.png)

C02 maps source, destination and rule target rather than editing pitches.
The [counter and reset diagrams](monome.md) explain the state those rules act on.
Proposed lesson: a specialist counter editor should expose both current state
and causal relationships.

## Provenance, storage and limits

[The original catalogue](visual_sources_20261117.json) and
[OXI supplement](oxi_sources_20261117.json) record every original URL, source
page, pinned commit where available, byte count, SHA-256, local path, inspection
and analysis link. Mutable/Monome/PER|FORMER/Bespoke/Stochas published sites are
dated snapshots, not guaranteed to match the separately pinned code revision.

Original image bytes are stored in
`docs/references/ui/sequencers_20261117/` and `docs/references/ui/oxi_20261117/`,
and kept out of app artwork and Git.
The analysis and catalogue are tracked. A Git checkout alone therefore does not
contain the illustrations; use the separately verified visual-reference backup
listed in [the engineering log](../../project/ENGINEERING_LOG.md), or retrieve
the original URLs and verify their hashes. Future upstream changes can make
re-downloading a different snapshot insufficient.

These references support visual hierarchy and documented workflows. They do not
prove hands-on usability, accessibility, host timing or performance. Code licences
and artwork permissions are separate; no reference artwork is adopted into Optisynth.
Next steps and unresolved states are explicit in [the workflow study](WORKFLOWS.md).
