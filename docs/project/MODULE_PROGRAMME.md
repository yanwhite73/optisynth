# Module programme: faithful reproductions of sophisticated modules

**This is a standing request.** Later requests add to it; they do not cancel it.
Every agent checks this list before choosing the next piece of work and records
progress here.

Yan has asked for this repeatedly:
- **17 November,** requests 8, 12 and 18: start with the Mutable modules, with
  interfaces like the originals.
- **22 November,** request 81: "just build clones of the make noise and 4ms or
  mutable devices".
- **23 November,** request 87: "we need much better clones or versions of modules".
- **24 November,** requests 96–98: "all of the important mutable and 4ms open source
  modules should get built, and especially complex delays and reverbs, we need at
  least one device as deep as starlab, sealegs, cosmic debris etc", and Make Noise
  too.
- **24 November,** request 115: deep modules like Mimeophon and Vhikk X, original
  layouts, and inputs and outputs that can be seen.
- **26 November,** request 136: "I have asked several times and it seems to get
  ignored".

Background:
- the evidence and licences are in
  [MODULE_FIDELITY_REVIEW.md](../research/modules/MODULE_FIDELITY_REVIEW.md);
- the prevention record is in [ENGINEERING_LOG.md](ENGINEERING_LOG.md), request 136.

## What counts as done

- **DSP.** Use the original open-source DSP at a pinned revision, not a lookalike.
  For closed designs, build a model from the manual and label it that way.
- **Controls and patching.** Reproduce the original's whole control set: modes,
  inputs, outputs and the normalling between sockets.
- **Panel.** Follow the original's arrangement (large / medium / attenuverter, and
  sockets at the bottom), drawn by us and under our own name.
- **Checks.**
  - The module's DSP is compared offline with the upstream code's behaviour.
  - No allocation on the audio thread.
  - CPU is measured.
- **Demonstration.** At least one factory patch shows what the module is for.

## Order and status

| # | Module | Basis | Status |
| --- | --- | --- | --- |
| 1 | **Mala**: Beads granular / delay / reverb, with its panel | Mutable Beads source, MIT, unchanged | **Built and checked** on branch `mala` (26 November): offline, sanitizer and native checks pass. Merges into the next release after Codex's 0.0.35. [Design and checks](../research/modules/MALA.md) |
| 2 | Complete Plaits-derived macro oscillator, all models | Mutable Plaits source, MIT | Not started. Three model cores already wrapped |
| 3 | Rings-derived resonator, then Elements | Mutable source, MIT | Not started |
| 4 | Clouds, all modes, including the Parasites modes | Mutable source / Parasites, MIT | One mode (the reverb tank) inside Prism tank |
| 5 | Looping delay on the real 4ms DLD firmware | 4ms DLD source, MIT | Manual-based study exists; firmware not ported |
| 6 | 4ms Spectral Multiband Resonator; Ensemble Oscillator | 4ms source, MIT / BSD-2 | Not started |
| 7 | Mimeophon-style repeater | Make Noise manual (closed DSP) | Not started |
| 8 | StarLab / Sealegs / Erbe-Verb-class depth targets | Manuals (closed DSP) | Not started |
| 9 | Vhikk X-inspired voice | Needs the manual or recordings | Waiting |
