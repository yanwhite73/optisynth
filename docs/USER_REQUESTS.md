# Optisynth — verbatim user inputs

Captured 17 November 2026, in conversation order. Wording and spelling are
preserved. Later additions steer the same project unless explicitly cancelled.
Implementation status lives in `STATUS.md`; research lives in `RESEARCH.md`.

## 1 — Multi-engine instrument and agentic composition

```text
how feasible is it to branch this into a multi-engine synth that can also use samples, granular, modes emulating JP8080, Oberheim xpander, mutable instruments modules etc?
Having my own modular environment would make composing with agentic tools much easier
```

## 2 — Physical modelling and hybrid percussion

```text
I would also add to that a virtual modelling mode mainly for percussion and drums, perhaps using samples alongside or with resonators
```

## 3 — Shared, routable components

```text
Having parts of each synth engine modular in a way that they can share elements like envellopes or fx, and routable would be amazing
```

## 4 — Separate project and modular Spectral rebuild

```text
If you start work on this, clone the repo and we would start on a new project and rebuild spectral so some if its acrhitecture is modular
```

## 5 — Initial working folder

```text
for now im calling it /Users/yanwhite/Documents/Music\ docs/Code/multisynth
```

## 6 — Manuals and schematics research

```text
part of this should also be a research project which would involve finding schematics and manuals for aforementioned classic analog synths
```

## 7 — Performance and loading only used modules

```text
the architecture should be performance-friendly, escpeically if made into a vst, so probably only load modules as they are used
```

## 8 — Mutable source and existing ports

```text
mutable source should be available open-source and there are plenty of clones and vcv rack devices to look at for pointers
```

## 9 — Project-local research documentation

```text
collect reference and research in docs in the folder. Documentation is necessary for a successful project
```

## 10 — Verbatim input capture

```text
including as always verbatim my inputs
```

## 11 — Project renamed and GitHub repository supplied

```text
renamed to optisynth
https://github.com/yanwhite73/optisynth.git
```

## 12 — Mutable first; interchangeable envelopes/filters; shared environment

```text
start with mutable modules, and have multiple options for envelopes and filters, starting with the ones from spectral. I am thinking along modular lines, but an environment, standalone synth and possibly vst which can load all these different modules and engines
```

## 13 — Contextual module interfaces

```text
UI matters a lot here as well. Use vcv modules as a guide or the mutable pages / original modules. Loading a different oscillator or resonator should load contextual ui for that module
```

## 14 — Circuit modelling question

```text
would you need to model circuits for the mutable code for it to sound good?
```

## 15 — Combined envelope routes sound rough

```text
sounds good so far, nice tones, some aliasing when both checkboxes selected
```

## 16 — Defer sound-quality and performance refinement

```text
maybe just log it, lets move quickly and refine performance and sound wquality later
```

## 17 — Keep building

```text
Ok dont stop
```

## 18 — Prioritize visible Mutable modules

```text
I'd like to see the mutable modules soon, osc, filter, etc
```

## 19 — Use available open-source implementations

```text
whichever ones have open source code
```

## 20 — Other respected makers with open digital-module code

```text
which other well respected modular builders release open source code for digital modules that we could use?
```

## 21 — Log research and back up through Git regularly

```text
great log all this and make sure to commit and push regularly
```

This explicitly supersedes the inherited no-commit/no-push restriction for
Optisynth. Commit and push coherent, checked milestones to the supplied origin;
retain local source archives as an independent backup.

## 22 — Parallel Wavestate repurposing project

```text
add another note, a parallel project to repurpose a korg wavestate might become a useful controller or even host
```
