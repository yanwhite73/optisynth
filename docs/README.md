# Optisynth documentation

Start with [current status](project/STATUS.md), [user requests](project/USER_REQUESTS.md)
and the [engineering log](project/ENGINEERING_LOG.md).

Sequencer UI research: [illustrated atlas with 39 references](ui/sequencers/README.md)
and [workflow comparison](ui/sequencers/WORKFLOWS.md).

| Folder | Contents | Start here |
| --- | --- | --- |
| `project/` | Status, verbatim requests, engineering history, backups and document relocation record | [Status](project/STATUS.md) |
| `architecture/` | Engine, graph/routing, sample loading and complete Spectral integration | [Architecture](architecture/ARCHITECTURE.md) |
| `ui/` | Contextual panels, interaction design, sequencer UI and visual-reference coverage | [UI research](ui/README.md) |
| `research/modules/` | Mutable integration review, other builders, delay/reverb candidates | [Module makers](research/modules/OPEN_SOURCE_MODULE_MAKERS.md) |
| `research/sequencers/` | Sequencer model/method comparisons, earlier Optiseq findings and source manifest | [Sequencer research](research/sequencers/SEQUENCER_MODELS_RESEARCH.md) |
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
