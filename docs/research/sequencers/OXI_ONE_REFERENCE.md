# OXI ONE: model and interface reference

17 November 2026 · request 42 · [Illustrated comparison](../../ui/sequencers/oxi.md)

The user's “ox sequencer” is interpreted as **OXI ONE**. The inspected reference
is specifically the **MKII**, using official HTML manual pages labelled firmware
2.2.10 / manual v2.0a. Keep this version separate from the original ONE and older
six-mode descriptions. The current overview lists eight sequencer instances and
nine modes. [Official mode overview](https://docs.oxiinstruments.com/oxi-one-mkii-manual/sequencer-modes/).

## Source status and collection

OXI belongs in the interface and musical-model research even though reusable ONE
engine source is not established. The manufacturer's public organisation includes
an E16 SDK, but that is a different product. The visible ONE MKII repository
contains a README describing beta releases and issue tracking; this does not grant
a licence to reuse a sequencer implementation.
[Manufacturer repositories](https://github.com/OXI-Instruments),
[ONE MKII release repository](https://github.com/OXI-Instruments/OXI-One-MkII-Releases).

Saved 13 official manual chapter snapshots and 15 original interface/behaviour
diagrams. All 15 images were visually inspected and annotated. Exact URLs, hashes,
edition and selected inspection scope are in the
[OXI catalogue](../../ui/sequencers/oxi_sources_20261117.json). Raw references are
under `docs/references/ui/oxi_20261117/`, ignored by Git and separately backed up.
No firmware, desktop app, MIDI client or device was installed, built or run.
This is selected documentation/visual study, not a complete pre-adoption review.

## Models: what the common grid does and does not share

| Mode | Documented musical model | Visual evidence | Optisynth synthesis |
| --- | --- | --- | --- |
| Mono | One note per step; note rows and time columns | OX01 | Pattern entry constraint; preserve content when changing compatible views |
| Poly | Several independently entered notes per step, with note parameters | OX02 | Same event editor with polyphonic content, not a new grid implementation |
| Multitrack | Eight independent monophonic tracks; each can have its own length/rate/output | OX03 | Pattern track view with explicit headers and destinations |
| Chord | Steps represent chord choices and voicing rather than only individual notes | OX04–05 | Typed chord progression editor plus palette; show rendered notes without discarding harmonic intent |
| Stochastic | Probability controls feed an evolving phrase; the visible grid is not its timeline | OX06 | Stateful generator with separate controls, output history, lock and capture |
| Matriceal | Four tracks of independently sequenced parameter lanes | OX07 | Advanced independent-lane editor with explicit trigger and update relationships |
| Domino | A trigger sequence advances a separate note sequence | OX08 | Trigger-driven phrase traversal, with separate cursors and an explicit advance source |
| Saga | Stages have their own durations and may contain notes or chords | OX09–10 | Stage module; distinguish stage count, elapsed duration and gate length |
| MIDI Player | Listed as file playback in the current overview/product page | No dedicated image in this collection | Keep imported-event playback separate from procedural generation; detailed study deferred |

Primary mode pages: [Mono](https://docs.oxiinstruments.com/oxi-one-mkii-manual/mono-mode/),
[Poly](https://docs.oxiinstruments.com/oxi-one-mkii-manual/poly-mode/),
[Multitrack](https://docs.oxiinstruments.com/oxi-one-mkii-manual/multitrack-mode/),
[Chord](https://docs.oxiinstruments.com/oxi-one-mkii-manual/chord-mode/),
[Stochastic](https://docs.oxiinstruments.com/oxi-one-mkii-manual/stochastic-mode/),
[Matriceal](https://docs.oxiinstruments.com/oxi-one-mkii-manual/matriceal-mode/),
[Domino](https://docs.oxiinstruments.com/oxi-one-mkii-manual/domino-mode/),
[Saga](https://docs.oxiinstruments.com/oxi-one-mkii-manual/saga-mode/).
MIDI Player listing: [official product page](https://oxiinstruments.com/oxi-one-mkii).

The shared hardware surface is not evidence that every mode should share identical
saved data or playback state. In particular, a chord definition, remembered random
phrase, independently running parameter lanes and trigger-driven note cursor have
different invariants. This supports the proposed shared frame and specialist models.

## Workflow findings that change the proposal

**Harmonic editing deserves a first-class surface.** OXI separates chord selection
and voicing from freely entered Poly notes; its manual also describes copying Chord
material into Poly for further editing. Our proposal should include an editable
chord lane/palette with root, quality, inversion/voicing, register and duration.
“Render to notes” creates an editable result while retaining its source. An inferred
chord label over arbitrary notes is not an equivalent representation.
[Chord manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/chord-mode/),
[Poly manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/poly-mode/).

**A phrase can advance from another rhythm.** Domino's trigger events, including
trigger repeats, advance the note layer. Our lane contract needs an advance policy
and separate note/trigger positions; a rest must not automatically consume the next
pitch unless that policy says so. Matriceal also makes parameter-lane timing and
trigger relationships explicit. Do not treat every independent lane as an ordinary
fixed-rate automation strip.
[Domino manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/domino-mode/),
[Matriceal manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/matriceal-mode/).

**The arp's input must be visible.** OXI Poly can arpeggiate notes already on its
sequence grid, whereas keyboard view selects the keyboard arp. In Optisynth the
independent Arp should accept either a live keyboard stream or an upstream Pattern/
Chord stream through an explicit connection. Show the current source and held-note
set; hidden view-dependent input selection would make agentic composition confusing.
[Poly arpeggiator section](https://docs.oxiinstruments.com/oxi-one-mkii-manual/poly-mode/).

**Harmonic following is a relationship, not a transpose amount.** OXI's Harmonizer
uses a Chord/Poly leader while followers retain rhythmic patterns. For Optisynth,
research a typed harmonic-context connection or an explicit harmoniser module with
leader, current chord/scale, range and bypass visible. This is a proposed extension,
not a promise to reproduce OXI's undocumented internal mapping.
[Harmonizer manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/harmonizer/).

**Global performance state must remain discoverable.** The OXI performance page
gives each sequencer a model-dependent row. The manual warns that its mutes persist
after leaving the page and can explain apparently silent sequences. Our editors
should retain a performance-mute/transpose badge and a way back to the source of
that state. Snapshot/restore should be explicit and separately scoped from edit
undo; OXI's snapshot covers notes, settings, mutes and modulation.
[Performance manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/performance-tools/).

**Arrangement needs separate editing and playback state.** Opening OXI's arranger
does not arm song playback. Its grid distinguishes queued, sounding, intentionally
blank and unused slots (OX11). Those are useful states for Conductor. Use readable
names and separate selection/armed indicators on desktop, with an explicit policy
for applying unsaved phrase edits at musical boundaries.
[Arranger manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/arranger/).

**Control input and effective output are different displays.** OX13 distinguishes
entered modulation values from output after offset, and its lane timing is separate
from note timing. Our control editor should expose destination units, base values,
modulation and effective output without forcing MIDI's numeric range on internal
synth parameters.
[Modulation manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/modulation/).

## Limits and questions for our wireframes

The overview capability table marks Poly accumulator unavailable, but the Poly
chapter says it has an accumulator per step. Record that inconsistency rather
than deriving a definitive compatibility table from one page. Our capability list
must be generated from actual supported behaviour when implemented.
[Overview](https://docs.oxiinstruments.com/oxi-one-mkii-manual/sequencer-modes/),
[Poly step options](https://docs.oxiinstruments.com/oxi-one-mkii-manual/poly-mode/).

The images explain a hardware grid, not mouse/keyboard usability. Desktop proposals
must test selecting a track without adding a note, editing chord quality while
seeing voiced notes, explaining a stopped note cursor under an advancing trigger
cursor, restoring a performance snapshot, and finding a persistent mute. Different
axes need persistent labels. No hardware audition, firmware algorithm verification
or comparative performance measurement is claimed. Original graphics are reference
material, not adopted Optisynth artwork.
