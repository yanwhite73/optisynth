# Bespoke: compact modules with distinct musical models

[Visual atlas](README.md) · [Workflow comparison](WORKFLOWS.md)

Published reference images, visually inspected on 17 November 2026. **Observed**
means visible in the image; **Optisynth inference** is our proposed adaptation,
not a claim about the upstream implementation. Open an image for its original size.

## B01 — Note sequencer

[![B01: Note sequencer](../../references/ui/sequencers_20261117/B01-note-sequencer.png)](../../references/ui/sequencers_20261117/B01-note-sequencer.png)

published screenshot. [Original image](https://www.bespokesynth.com/docs/screenshots/notesequencer.png) · [Publisher documentation](https://www.bespokesynth.com/docs/index.html#notesequencer).

**Observed:** Pitch-labelled rows and step columns share a small panel with pitch/velocity/length choices and octave/scale controls.

**Optisynth inference:** A compact rack preview can offer common note edits; use a larger focus editor for detailed chords and timing.

## B02 — Drum sequencer

[![B02: Drum sequencer](../../references/ui/sequencers_20261117/B02-drum-sequencer.png)](../../references/ui/sequencers_20261117/B02-drum-sequencer.png)

published screenshot. [Original image](https://www.bespokesynth.com/docs/screenshots/drumsequencer.png) · [Publisher documentation](https://www.bespokesynth.com/docs/index.html#drumsequencer).

**Observed:** An eight-row drum grid has separate length, step, randomisation and offset controls. Rows are numbered rather than instrument-named.

**Optisynth inference:** Use named drum destinations and keep per-row selection. Advanced randomisation should not crowd basic hit entry.

## B03 — Euclidean rings

[![B03: Euclidean rings](../../references/ui/sequencers_20261117/B03-euclidean-rings.png)](../../references/ui/sequencers_20261117/B03-euclidean-rings.png)

published screenshot. [Original image](https://www.bespokesynth.com/docs/screenshots/euclideansequencer.png) · [Publisher documentation](https://www.bespokesynth.com/docs/index.html#euclideansequencer).

**Observed:** Four coloured concentric rings correspond to rows of steps, onsets, rotation, offset and note controls. Random ranges occupy much of the header.

**Optisynth inference:** Pair rhythm geometry with exact numbers. Give the selected ring a label and outline; move secondary random controls into an inspector.

## B04 — Variable duration stages

[![B04: Variable duration stages](../../references/ui/sequencers_20261117/B04-variable-duration-stages.png)](../../references/ui/sequencers_20261117/B04-variable-duration-stages.png)

published screenshot. [Original image](https://www.bespokesynth.com/docs/screenshots/m185sequencer.png) · [Publisher documentation](https://www.bespokesynth.com/docs/index.html#m185sequencer).

**Observed:** Eight stages each expose pitch, pulse count and gate mode. The published empty example shows zero pulses and total steps zero.

**Optisynth inference:** Make stage dwell visible beside gate behaviour. Provide an audible starter and explain zero-duration stages; do not confuse dwell with ratchets.

## B05 — Independent arpeggiator

[![B05: Independent arpeggiator](../../references/ui/sequencers_20261117/B05-independent-arpeggiator.png)](../../references/ui/sequencers_20261117/B05-independent-arpeggiator.png)

published screenshot. [Original image](https://www.bespokesynth.com/docs/screenshots/arpeggiator.png) · [Publisher documentation](https://www.bespokesynth.com/docs/index.html#arpeggiator).

**Observed:** A tiny arpeggiator presents interval, step and octave settings with no visible held-note list.

**Optisynth inference:** Use this compactness in the rack, but show incoming/latched notes and output order in focus so an idle arp is understandable.

## B06 — Control sequence

[![B06: Control sequence](../../references/ui/sequencers_20261117/B06-control-sequence.png)](../../references/ui/sequencers_20261117/B06-control-sequence.png)

published screenshot. [Original image](https://www.bespokesynth.com/docs/screenshots/controlsequencer.png) · [Publisher documentation](https://www.bespokesynth.com/docs/index.html#controlsequencer).

**Observed:** Eight value faders sit below length, interval, random and record controls.

**Optisynth inference:** Reuse fader widgets for control sequences, with destination units, bipolar range and interpolation visible.

## B07 — Piano roll canvas

[![B07: Piano roll canvas](../../references/ui/sequencers_20261117/B07-piano-roll-canvas.png)](../../references/ui/sequencers_20261117/B07-piano-roll-canvas.png)

published screenshot. [Original image](https://www.bespokesynth.com/docs/screenshots/notecanvas.png) · [Publisher documentation](https://www.bespokesynth.com/docs/index.html#notecanvas).

**Observed:** The piano-roll canvas has loop/ruler controls, recording, quantisation and MIDI file controls, plus an empty pitch grid.

**Optisynth inference:** Preserve familiar duration editing in focus. An empty state should explain note entry; recording, drawing and selection need distinct feedback.

## B08 — Song builder

[![B08: Song builder](../../references/ui/sequencers_20261117/B08-song-builder.png)](../../references/ui/sequencers_20261117/B08-song-builder.png)

published screenshot. [Original image](https://www.bespokesynth.com/docs/screenshots/songbuilder.png) · [Publisher documentation](https://www.bespokesynth.com/docs/index.html#songbuilder).

**Observed:** Named song sections form a vertical list with launch triangles, per-row menus and an Add Section button.

**Optisynth inference:** Start Conductor with an understandable scene/phrase list. Complex branching can be a separate advanced editor.
