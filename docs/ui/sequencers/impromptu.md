# Impromptu: immediate rack interaction and performance

[Visual atlas](README.md) · [Workflow comparison](WORKFLOWS.md)

Published reference images, visually inspected on 17 November 2026. **Observed**
means visible in the image; **Optisynth inference** is our proposed adaptation,
not a claim about the upstream implementation. Open an image for its original size.

## I01 — Foundry panel

[![I01: Foundry panel](../../references/ui/sequencers_20261117/I01-foundry-panel.jpg)](../../references/ui/sequencers_20261117/I01-foundry-panel.jpg)

published screenshot. [Original image](https://raw.githubusercontent.com/MarcBoule/ImpromptuModular/cf87c918875e502043cabe3deaa2e52adda7cecd/res/img/Foundry.jpg) · [Publisher documentation](https://marcboule.github.io/ImpromptuModular/#foundry).

**Observed:** Foundry groups step buttons, a small pitch keyboard, sequence/phrase/track displays and four clock/CV/gate output groups on one panel. Several controls have dual labels.

**Optisynth inference:** Keep model-specific pitch entry and immediate performance controls. In focus, separate song versus sequence editing and expose full names.

## I02 — PhraseSeq16 panel

[![I02: PhraseSeq16 panel](../../references/ui/sequencers_20261117/I02-phraseseq16-panel.jpg)](../../references/ui/sequencers_20261117/I02-phraseseq16-panel.jpg)

published screenshot. [Original image](https://raw.githubusercontent.com/MarcBoule/ImpromptuModular/cf87c918875e502043cabe3deaa2e52adda7cecd/res/img/PhraseSeq16.jpg) · [Publisher documentation](https://marcboule.github.io/ImpromptuModular/#phraseseq16).

**Observed:** PhraseSeq16 reduces the same vocabulary to one step strip, pitch keyboard, sequence/song switch, copy/paste and gate/slide controls.

**Optisynth inference:** Shared interaction vocabulary can survive a smaller panel. Avoid reproducing every overloaded hardware gesture on a desktop.

## I03 — GateSeq64 panel

[![I03: GateSeq64 panel](../../references/ui/sequencers_20261117/I03-gateseq64-panel.jpg)](../../references/ui/sequencers_20261117/I03-gateseq64-panel.jpg)

published screenshot. [Original image](https://raw.githubusercontent.com/MarcBoule/ImpromptuModular/cf87c918875e502043cabe3deaa2e52adda7cecd/res/img/GateSeq64.jpg) · [Publisher documentation](https://marcboule.github.io/ImpromptuModular/#gateseq64).

**Observed:** GateSeq64 presents four rows of 16 buttons, gate-shape choices and a 4x16/2x32/1x64 selector.

**Optisynth inference:** A gate-only editor should be dense and readable without pitch controls. Show lane grouping and loop lengths directly when the layout changes.

## I04 — BigButtonSeq2 panel

[![I04: BigButtonSeq2 panel](../../references/ui/sequencers_20261117/I04-bigbuttonseq2-panel.jpg)](../../references/ui/sequencers_20261117/I04-bigbuttonseq2-panel.jpg)

published screenshot. [Original image](https://raw.githubusercontent.com/MarcBoule/ImpromptuModular/cf87c918875e502043cabe3deaa2e52adda7cecd/res/img/BigButtonSeq2.jpg) · [Publisher documentation](https://marcboule.github.io/ImpromptuModular/#bigbuttonseq2).

**Observed:** BigButtonSeq2 gives one recording button much more area than its channel, length, bank, delete, fill and clear controls.

**Optisynth inference:** Performance capture is a different task from precise grid editing. Give recording a clear state and separate destructive clear from fill.
