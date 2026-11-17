# Seq: a stable workspace around different composition editors

[Visual atlas](README.md) · [Workflow comparison](WORKFLOWS.md)

Published reference images, visually inspected on 17 November 2026. **Observed**
means visible in the image; **Optisynth inference** is our proposed adaptation,
not a claim about the upstream implementation. Open an image for its original size.

## S01 — Step sequence and inspectors

[![S01: Step sequence and inspectors](../../references/ui/sequencers_20261117/S01-step-sequence-and-inspectors.png)](../../references/ui/sequencers_20261117/S01-step-sequence-and-inspectors.png)

published screenshot. [Original image](https://raw.githubusercontent.com/eclab/seq/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/stepsequence.png) · [Publisher documentation](https://github.com/eclab/seq/blob/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/manual.tex).

**Observed:** A motif browser stays on the left, a multi-track step grid occupies the centre, and Sequence/Track/Step inspectors stack on the right. Inherited values appear as defaults.

**Optisynth inference:** Make edit scope and inherited versus overridden values explicit. Bulk operations must name their scope and remain undoable.

## S02 — Note events and lanes

[![S02: Note events and lanes](../../references/ui/sequencers_20261117/S02-note-events-and-lanes.png)](../../references/ui/sequencers_20261117/S02-note-events-and-lanes.png)

published screenshot. [Original image](https://raw.githubusercontent.com/eclab/seq/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/Notes.png) · [Publisher documentation](https://github.com/eclab/seq/blob/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/manual.tex).

**Observed:** The same browser and inspector frame contains a piano roll with controller lanes above it and detailed selected-note properties on the right.

**Optisynth inference:** Reuse the workspace, selection and inspector contracts across views. Keep controller lanes typed and preserve note durations.

## S03 — Serial phrase composition

[![S03: Serial phrase composition](../../references/ui/sequencers_20261117/S03-serial-phrase-composition.png)](../../references/ui/sequencers_20261117/S03-serial-phrase-composition.png)

published screenshot. [Original image](https://raw.githubusercontent.com/eclab/seq/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/Series.png) · [Publisher documentation](https://github.com/eclab/seq/blob/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/manual.tex).

**Observed:** A serial child list contains repeated motif names while the right inspector exposes child nickname, repeats, rate, transpose and selection weights.

**Optisynth inference:** Distinguish a reusable phrase definition from each occurrence. Put occurrence transforms beside the arrangement item; do not imply these weights are learned musical transitions.

## S04 — Parallel phrase composition

[![S04: Parallel phrase composition](../../references/ui/sequencers_20261117/S04-parallel-phrase-composition.png)](../../references/ui/sequencers_20261117/S04-parallel-phrase-composition.png)

published screenshot. [Original image](https://raw.githubusercontent.com/eclab/seq/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/Parallel.png) · [Publisher documentation](https://github.com/eclab/seq/blob/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/manual.tex).

**Observed:** Parallel children are drawn as overlapping, offset horizontal spans. A child inspector exposes delay, probability, rate, transpose, gain and output.

**Optisynth inference:** A shared composition frame can change its central representation to show simultaneity. Time offsets need exact units and a readable ruler in our version.

## S05 — Branching composition

[![S05: Branching composition](../../references/ui/sequencers_20261117/S05-branching-composition.png)](../../references/ui/sequencers_20261117/S05-branching-composition.png)

published screenshot. [Original image](https://raw.githubusercontent.com/eclab/seq/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/Automaton.png) · [Publisher documentation](https://github.com/eclab/seq/blob/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/manual.tex).

**Observed:** A spatial node grid connects named motifs through Fork, Join, Random and Iterate nodes. The selected node has a separate inspector.

**Optisynth inference:** Reserve graph editing for actual branching. Keep these composition links visually distinct from audio patch cables and provide an active-path explanation.

## S06 — Arpeggio motif

[![S06: Arpeggio motif](../../references/ui/sequencers_20261117/S06-arpeggio-motif.png)](../../references/ui/sequencers_20261117/S06-arpeggio-motif.png)

published screenshot. [Original image](https://raw.githubusercontent.com/eclab/seq/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/Arpeggio.png) · [Publisher documentation](https://github.com/eclab/seq/blob/932cb70fb9b4f68621e45f49d41c8331a4fa69ac/docs/manual.tex).

**Observed:** The arpeggio editor displays relative pitch movement across a grid beside type, octaves, length and chord-reset options.

**Optisynth inference:** Preview note order alongside arp controls. Label whether a grid is an editable definition or generated preview and show its input chord separately.
