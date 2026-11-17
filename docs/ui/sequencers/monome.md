# Monome: parameter lanes, counters and explicit rules

[Visual atlas](README.md) · [Workflow comparison](WORKFLOWS.md)

Published reference images, visually inspected on 17 November 2026. **Observed**
means visible in the image; **Optisynth inference** is our proposed adaptation,
not a claim about the upstream implementation. Open an image for its original size.

## K00 — Kria navigation

[![K00: Kria navigation](../../references/ui/sequencers_20261117/K00-kria-navigation.png)](../../references/ui/sequencers_20261117/K00-kria-navigation.png)

manual interaction diagram. [Original image](https://monome.org/docs/ansible/images/grid_KR_NAVIGATION_1.2.png) · [Publisher documentation](https://monome.org/docs/ansible/kria/).

**Observed:** The annotated navigation row separates track, parameter, modifier, scale and pattern choices.

**Optisynth inference:** Keep these as distinct levels of navigation and avoid making users memorise unlabelled pad locations.

## K01 — Kria parameter and loop overview

[![K01: Kria parameter and loop overview](../../references/ui/sequencers_20261117/K01-kria-parameter-and-loop-overview.png)](../../references/ui/sequencers_20261117/K01-kria-parameter-and-loop-overview.png)

manual interaction diagram. [Original image](https://monome.org/docs/ansible/images/grid_KR_overview.png) · [Publisher documentation](https://monome.org/docs/ansible/kria/).

**Observed:** The overview labels direction, clock choices and parameter/modifier controls around one grid.

**Optisynth inference:** Independent lane settings need visible names, loop extents and rate. A single shared playhead cannot explain different lane phases.

## K02 — Kria meta pattern sequence

[![K02: Kria meta pattern sequence](../../references/ui/sequencers_20261117/K02-kria-meta-pattern-sequence.png)](../../references/ui/sequencers_20261117/K02-kria-meta-pattern-sequence.png)

manual interaction diagram. [Original image](https://monome.org/docs/ansible/images/grid_KR_METAPattern.png) · [Publisher documentation](https://monome.org/docs/ansible/kria/).

**Observed:** A second grid view labels pattern list, clock divider, pattern sequence and meta-sequence; its annotation explains a held-button entry gesture.

**Optisynth inference:** Expose pattern arrangement as a named destination. Keep musical hierarchy while replacing hidden hardware chords with visible desktop navigation.

## C00 — Meadowphysics counter anatomy

[![C00: Meadowphysics counter anatomy](../../references/ui/sequencers_20261117/C00-meadowphysics-counter-anatomy.png)](../../references/ui/sequencers_20261117/C00-meadowphysics-counter-anatomy.png)

manual interaction diagram. [Original image](https://monome.org/docs/ansible/images/grid_MP_OVERVIEW_1.1.png) · [Publisher documentation](https://monome.org/docs/ansible/meadowphysics/).

**Observed:** The diagram separates countdown position, expiry event, count range and reset destination.

**Optisynth inference:** Use a counter strip with remaining count and reset range. A row of note values would communicate the wrong model.

## C01 — Meadowphysics reset output speed

[![C01: Meadowphysics reset output speed](../../references/ui/sequencers_20261117/C01-meadowphysics-reset-output-speed.png)](../../references/ui/sequencers_20261117/C01-meadowphysics-reset-output-speed.png)

manual interaction diagram. [Original image](https://monome.org/docs/ansible/images/grid_MP_RESET-OUTPUT-SPEEDS_1.2.png) · [Publisher documentation](https://monome.org/docs/ansible/meadowphysics/).

**Observed:** Holding a row exposes start/stop, reset, gate/trigger action and speed choices in the diagram.

**Optisynth inference:** Selecting a counter should reveal its timing and expiry inspector without hiding all other counters.

## C02 — Meadowphysics rule selection

[![C02: Meadowphysics rule selection](../../references/ui/sequencers_20261117/C02-meadowphysics-rule-selection.png)](../../references/ui/sequencers_20261117/C02-meadowphysics-rule-selection.png)

manual interaction diagram. [Original image](https://monome.org/docs/ansible/images/grid_MP_RULE_GLYPHS_1.2.png) · [Publisher documentation](https://monome.org/docs/ansible/meadowphysics/).

**Observed:** The rule view separates source, destination and position/speed targets and supplies eight labelled rule glyphs.

**Optisynth inference:** Show a relation matrix plus a readable rule sentence. Include the last fired rule so the cause of an event can be understood.
