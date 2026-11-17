# OXI ONE MKII: contextual grid modes and performance

17 November 2026 · request 42 · [Visual atlas](README.md) ·
[Model/source assessment](../../research/sequencers/OXI_ONE_REFERENCE.md)

“OX” is interpreted as OXI ONE. This page uses the manufacturer's MKII manual,
which identifies firmware 2.2.10 / manual v2.0a. It does not attribute MKII features
to the original ONE. These **15 original manual diagrams** were saved and visually
inspected; they are not photographs or live captures from our own hardware.

OXI is a useful test of the proposal: the physical grid stays constant, while the
meaning of a row, column and pad changes with the musical task. Our desktop editor
should retain familiar navigation but explicitly label those changing meanings.
The [assessment](../../research/sequencers/OXI_ONE_REFERENCE.md) records the model
distinctions, documented workflows, source limitations and consequences for Optisynth.

**Observed** describes the image. **Optisynth inference** proposes our adaptation.
Each image links to its full-size original local copy and publisher source.

## OX01 — Mono note entry

[![OX01: Mono note entry](../../references/ui/oxi_20261117/OX01.webp)](../../references/ui/oxi_20261117/OX01.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/mono-mode/mono-mode-002.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/mono-mode/).

**Observed:** Pitch rows and time columns show one active pitch at each step, with scrolling instructions beside the grid.

**Optisynth inference:** Reuse the note editor for mono entry, with a visible voice constraint and a reversible conversion to polyphonic content.

## OX02 — Polyphonic note entry

[![OX02: Polyphonic note entry](../../references/ui/oxi_20261117/OX02.webp)](../../references/ui/oxi_20261117/OX02.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/poly-mode/poly-mode-002.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/poly-mode/).

**Observed:** The same axes now show several pitches per column; the diagram distinguishes the note starts and continuations.

**Optisynth inference:** Share selection and duration editing with Mono. Voice limits must be explicit; do not silently delete earlier notes when a limit is exceeded.

## OX03 — Multitrack grid and selection

[![OX03: Multitrack grid and selection](../../references/ui/oxi_20261117/OX03.webp)](../../references/ui/oxi_20261117/OX03.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/multitrack-mode/multitrack-mode-001.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/multitrack-mode/).

**Observed:** Rows now mean tracks. The first column also selects a track; held Mute changes the edge columns into mute/solo controls.

**Optisynth inference:** Use separate named track headers on desktop so selecting a track cannot accidentally add its first note.

## OX04 — Chord sequence

[![OX04: Chord sequence](../../references/ui/oxi_20261117/OX04.webp)](../../references/ui/oxi_20261117/OX04.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/chord-mode/chord-mode-002.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/chord-mode/).

**Observed:** Rows label scale-related chords with miniature keyboard voicings, while columns still represent musical time.

**Optisynth inference:** A chord progression editor should retain chord identity and expose the resulting notes, rather than storing only a flattened piano roll.

## OX05 — Chord palette

[![OX05: Chord palette](../../references/ui/oxi_20261117/OX05.webp)](../../references/ui/oxi_20261117/OX05.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/chord-mode/chord-mode-025.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/chord-mode/).

**Observed:** Three lower rows distinguish basic, extended and altered chord choices. A colour legend identifies qualities; modifier pads occupy a separate area.

**Optisynth inference:** Provide an auditionable chord palette with explicit names and voiced-note previews. Keep colour supplementary to text and interval labels.

## OX06 — Stochastic probability controls

[![OX06: Stochastic probability controls](../../references/ui/oxi_20261117/OX06.webp)](../../references/ui/oxi_20261117/OX06.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/stochastic-mode/stochastic-mode-001.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/stochastic-mode/).

**Observed:** Columns represent candidate notes and rows their probabilities. Four rightmost columns control octave range, division and pitch/rhythm randomisation.

**Optisynth inference:** Label the axes by model. Display generated phrase history separately; a moving timeline cursor over this probability grid can mislead.

## OX07 — Matriceal track and parameter layout

[![OX07: Matriceal track and parameter layout](../../references/ui/oxi_20261117/OX07.webp)](../../references/ui/oxi_20261117/OX07.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/matriceal-mode/matriceal-mode-004.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/matriceal-mode/).

**Observed:** Four track regions each contain a step area, ten parameter selectors and a track status row.

**Optisynth inference:** Retain independent lane sequencing but use named lane tabs, visible loop lengths and separate phase markers in the larger focus editor.

## OX08 — Domino trigger and note layers

[![OX08: Domino trigger and note layers](../../references/ui/oxi_20261117/OX08.webp)](../../references/ui/oxi_20261117/OX08.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/domino-mode/domino-mode-001.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/domino-mode/).

**Observed:** Note steps and trigger steps occupy separate regions with separately labelled cursors; a keyboard sits below.

**Optisynth inference:** Show the causal link between the rhythm lane and note-advance lane. An Advance source selector must distinguish every clock from accepted triggers.

## OX09 — Saga stages

[![OX09: Saga stages](../../references/ui/oxi_20261117/OX09.webp)](../../references/ui/oxi_20261117/OX09.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/saga-mode/saga-mode-001.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/saga-mode/).

**Observed:** Stages run across successive rows; active notes, configured length and current stage use different indications. The keyboard remains below.

**Optisynth inference:** Use stage cards with explicit duration and elapsed sub-position. The equal-size pad layout should not imply equal elapsed time.

## OX10 — Saga duration example

[![OX10: Saga duration example](../../references/ui/oxi_20261117/OX10.webp)](../../references/ui/oxi_20261117/OX10.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/saga-mode/saga-mode-012.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/saga-mode/).

**Observed:** The example adds three ticks to a 32-stage sequence because one stage lasts four ticks; an offset diagram is shown separately.

**Optisynth inference:** Display total cycle duration alongside stage count. Treat gate length, stage dwell and note timing offset as separate concepts.

## OX11 — Arranger slots and states

[![OX11: Arranger slots and states](../../references/ui/oxi_20261117/OX11.webp)](../../references/ui/oxi_20261117/OX11.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/arranger/arranger-003.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/arranger/).

**Observed:** The arranger labels instrument rows, section columns and distinct playing, queued, blank-spacer and unused cells.

**Optisynth inference:** Show pending launches explicitly and distinguish an intentional silent slot from an unused slot. Make opening the arrangement and arming playback separate actions.

## OX12 — Performance overview

[![OX12: Performance overview](../../references/ui/oxi_20261117/OX12.webp)](../../references/ui/oxi_20261117/OX12.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/performance-tools/performance-tools-062.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/performance-tools/).

**Observed:** Rows identify different sequencer models, and their control groupings differ within the shared performance grid.

**Optisynth inference:** A performance dashboard can share placement while adapting available actions to each loaded model. Keep persistent performance mutes visible outside this page.

## OX13 — Modulation values and output

[![OX13: Modulation values and output](../../references/ui/oxi_20261117/OX13.webp)](../../references/ui/oxi_20261117/OX13.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/modulation/modulation-016.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/modulation/).

**Observed:** The figure separates entered modulation values from resulting output after offset, with independent length/division controls.

**Optisynth inference:** Keep base value, modulation and effective output inspectable. Use an explicit bipolar axis and destination units in the desktop editor.

## OX14 — Harmonic leader and followers

[![OX14: Harmonic leader and followers](../../references/ui/oxi_20261117/OX14.webp)](../../references/ui/oxi_20261117/OX14.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/harmonizer/harmonizer-002.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/harmonizer/).

**Observed:** A leader feeds harmonic information to a follower that retains its own rhythmic pattern. Keyboard and sequencer inputs are shown separately.

**Optisynth inference:** Model harmonic following as an explicit relationship with an inspectable leader, scale/chord state and bypass; do not hide it inside a global transpose knob.

## OX15 — Snapshot status

[![OX15: Snapshot status](../../references/ui/oxi_20261117/OX15.webp)](../../references/ui/oxi_20261117/OX15.webp)

[Original diagram](https://docs.oxiinstruments.com/img/oxi-one-mkii/performance-tools/performance-tools-097.webp) · [Official manual](https://docs.oxiinstruments.com/oxi-one-mkii-manual/performance-tools/).

**Observed:** A camera symbol marks the snapshot state in the main status display.

**Optisynth inference:** Provide a visible performance snapshot with restore and discard actions whose scope is clear. Distinguish it from ordinary edit undo and preset saving.
