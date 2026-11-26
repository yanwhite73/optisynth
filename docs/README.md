# Optisynth documentation

Start with [current status](project/STATUS.md), [user requests](project/USER_REQUESTS.md)
and the [engineering log](project/ENGINEERING_LOG.md).

App delivery uses one stable location; the workflow and request 53 correction are
in [AGENTS](../AGENTS.md#build-delivery-discipline) and
[the engineering log](project/ENGINEERING_LOG.md#request-53-inconsistent-app-delivery--19-november-2026).

Sequencer UI research: [illustrated atlas with 54 references](ui/sequencers/README.md)
and [workflow comparison](ui/sequencers/WORKFLOWS.md).
Polyphony and analog pad modules: [voice-group contract](architecture/VOICE_GROUPS.md); [performance lessons from Arturia, Diva and Korg](research/platforms/PERFORMANCE_QUALITY_LESSONS.md).
Hardware-derived rack modules: [dual optical gate and dual looping delay](research/modules/GATE_AND_LOOP_DELAY.md).
Current sequencer implementation: [Probability and Orbit](architecture/REFERENCE_SEQUENCERS.md)
with [native visual evidence](ui/REFERENCE_SEQUENCERS_REVIEW.md), [four-track Performance sequencer](architecture/PERFORMANCE_SEQUENCER.md),
[Pattern / Motion](architecture/PATTERN_MOTION.md), and the
[Pattern, Euclidean and Stage foundation](architecture/FIRST_SEQUENCERS.md).
Standing module programme (requests 8–136): [faithful reproductions, order and status](project/MODULE_PROGRAMME.md).
Module fidelity (request 115): [how much open-source DSP is faithfully recreated, and what else can be approached](research/modules/MODULE_FIDELITY_REVIEW.md).
Handoff to Codex, 26 November (requests 115–117): [module panels and deep delay/reverb modules](project/HANDOFF_CLAUDE_TO_CODEX_20261126.md).
Interface (requests 71–72): [rack density, synth-panel view and preset browser](ui/SYNTH_PANEL_AND_PRESETS.md); [existing JUCE UI frameworks and code](research/platforms/JUCE_UI_OPTIONS.md).
Sound goals (request 44): [ambient Berlin School and analog pads](project/SOUND_GOALS.md);
[project review, 19 November](project/PROJECT_REVIEW_20261119.md).

| Folder | Contents | Start here |
| --- | --- | --- |
| `project/` | Status, verbatim requests, engineering history, backups and document relocation record | [Status](project/STATUS.md) |
| `architecture/` | Engine, graph/routing, sample loading and complete Spectral integration | [Architecture](architecture/ARCHITECTURE.md) |
| `ui/` | Contextual panels, interaction design, sequencer UI and visual-reference coverage | [UI research](ui/README.md) |
| `research/modules/` | Mutable integration review, other builders, delay/reverb candidates | [Module makers](research/modules/OPEN_SOURCE_MODULE_MAKERS.md) |
| `research/sequencers/` | Sequencer model/method comparisons, earlier Optiseq findings and source manifest | [Sequencer research](research/sequencers/SEQUENCER_MODELS_RESEARCH.md) |
| `research/instruments/` | Classic-synth circuit documentation, plugin emulations and open-source models (Oberheim, Prophet) | [Oberheim and Prophet sources](research/instruments/OBERHEIM_PROPHET_SOURCES.md) |
| `research/platforms/` | Lessons from modular environments and their performance architecture | [VCV lessons](research/platforms/VCV_ARCHITECTURE_LESSONS.md) |
| `research/` | Research overview, classic-synth manual/schematic catalogue and download receipts | [Research index](research/RESEARCH.md) |
| `verification/` | Historical build receipts and original source-snapshot provenance | [Source snapshot](verification/source_snapshot_20261117.json) |
| `legacy/optispectral/` | Inherited Spectral design, handoffs, parameter/export documentation and registries | [Archive notes](legacy/optispectral/README.md) |
| `references/` | Local downloaded manuals, diagrams, screenshots and source evidence; git-ignored | [Collection catalogue](research/RESEARCH.md) |

Put new documents in their topic folder. Keep current project instructions in
`project/`; implementation contracts in `architecture/`; interface/screenshots
analysis in `ui/`; evidence and source comparisons in `research/`. New raw reference
collections belong in named subfolders of `references/`, with their URL/hash
catalogue tracked alongside the related research. Do not put copied screenshots
or upstream source into application assets merely because they are references.

The [relocation record](project/document_locations_20261117.json) maps all 37 former
flat paths to their new locations. Historical JSON receipts and inherited documents
retain their original contents; paths inside them describe the layout at the time.
Current links, agent instructions and helper file paths use this folder structure.

No synth implementation or build accompanies this documentation reorganisation.

- [Pattern / Motion performance sequencer](architecture/PATTERN_MOTION.md) — request 49 implementation and playback contract.
