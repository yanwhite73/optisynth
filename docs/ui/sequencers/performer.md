# PER|FORMER: one frame, several editing and performance tasks

[Visual atlas](README.md) · [Workflow comparison](WORKFLOWS.md)

Published reference images, visually inspected on 17 November 2026. **Observed**
means visible in the image; **Optisynth inference** is our proposed adaptation,
not a claim about the upstream implementation. Open an image for its original size.

## P01 — Hardware and navigation

[![P01: Hardware and navigation](../../references/ui/sequencers_20261117/P01-hardware-and-navigation.png)](../../references/ui/sequencers_20261117/P01-hardware-and-navigation.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/user-interface.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** Persistent tempo, track, playing-pattern and editing-pattern labels sit above five contextual buttons. Physical track and step buttons also carry navigation labels.

**Optisynth inference:** Keep a stable header and explicit editing scope. Desktop controls should expose functions that hardware must compress into button combinations.

## P02 — Note step editing

[![P02: Note step editing](../../references/ui/sequencers_20261117/P02-note-step-editing.png)](../../references/ui/sequencers_20261117/P02-note-step-editing.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/page-note-steps.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** A gate row sits over numbered steps; the bottom strip selects gate, retrigger, length, note and condition layers.

**Optisynth inference:** Keep selection and time position stable when changing a lane property. Show the active layer in text, not only by colour.

## P03 — Curve step editing

[![P03: Curve step editing](../../references/ui/sequencers_20261117/P03-curve-step-editing.png)](../../references/ui/sequencers_20261117/P03-curve-step-editing.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/page-curve-steps.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** The same screen frame now contains individual curve shapes and shape/min/max/gate controls.

**Optisynth inference:** Reuse navigation around a genuinely typed control editor. Curves need range and interpolation controls instead of pitch widgets.

## P04 — Quick access parameters

[![P04: Quick access parameters](../../references/ui/sequencers_20261117/P04-quick-access-parameters.png)](../../references/ui/sequencers_20261117/P04-quick-access-parameters.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/page-note-steps-quick-access.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** A First Step quick-access overlay retains the surrounding step context.

**Optisynth inference:** Offer a small loop-bound inspector without replacing the pattern view; make temporary overlays dismissible and keyboard focus explicit.

## P05 — Sequence settings

[![P05: Sequence settings](../../references/ui/sequencers_20261117/P05-sequence-settings.png)](../../references/ui/sequencers_20261117/P05-sequence-settings.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/page-note-sequence.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** A list presents first/last step, run mode and divisor separately from the musical grid.

**Optisynth inference:** Keep advanced timing in an inspector, but display consequential loop bounds and rate beside the lane so hidden settings cannot explain unexpected silence.

## P06 — Pattern launch grid

[![P06: Pattern launch grid](../../references/ui/sequencers_20261117/P06-pattern-launch-grid.png)](../../references/ui/sequencers_20261117/P06-pattern-launch-grid.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/page-pattern.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** Eight track rows contain pattern cells; the lower strip exposes latch, sync and snapshot controls.

**Optisynth inference:** Pattern launch deserves its own surface. Distinguish the cell selected for editing from the pattern playing and any pending launch.

## P07 — Song chain example

[![P07: Song chain example](../../references/ui/sequencers_20261117/P07-song-chain-example.png)](../../references/ui/sequencers_20261117/P07-song-chain-example.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/page-song-chain-example.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** A song table has numbered rows, a count column and one pattern-number column per track.

**Optisynth inference:** An ordered phrase table is a sufficient first arrangement editor. Use readable pattern names and explicit repeats instead of requiring a graph.

## P08 — Song playback

[![P08: Song playback](../../references/ui/sequencers_20261117/P08-song-playback.png)](../../references/ui/sequencers_20261117/P08-song-playback.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/page-song-playback.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** The song playback example adds a row/progress indication and offers Stop in the same frame as P07.

**Optisynth inference:** Keep the arrangement layout stable during playback. Give progress and edit selection different markers, with optional Follow.

## P09 — Live performance controls

[![P09: Live performance controls](../../references/ui/sequencers_20261117/P09-live-performance-controls.png)](../../references/ui/sequencers_20261117/P09-live-performance-controls.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/page-performer.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** Track mute boxes, miniature event strips and fill bars share a dedicated performance page.

**Optisynth inference:** Group mute, fill and pending changes into a performance surface that can be used without selecting individual notes.

## P10 — Routing editor

[![P10: Routing editor](../../references/ui/sequencers_20261117/P10-routing-editor.png)](../../references/ui/sequencers_20261117/P10-routing-editor.png)

manual screen/panel diagram. [Original image](https://westlicht.github.io/performer/manual/images/page-routing-edit.png) · [Publisher documentation](https://westlicht.github.io/performer/manual/).

**Observed:** The routing editor lists target, minimum, maximum and track; the footer includes Learn and Commit.

**Optisynth inference:** Place destination and effective range together. A prepared routing edit needs an explicit applied state, not merely a moving source indicator.
