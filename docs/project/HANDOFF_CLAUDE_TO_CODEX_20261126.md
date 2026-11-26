# Handoff: module panels and deep delay/reverb modules (Claude → Codex), 26 November 2026

**From:** Claude Code session `94ced1d5-0393-4892-b942-0866ef5504ab`.
- Full transcript, if detail is needed:
  `~/.claude/projects/-Users-yanwhite-Documents-Music-docs-Code-optisynth/94ced1d5-0393-4892-b942-0866ef5504ab.jsonl`.
- Requests 115 and 116 in [USER_REQUESTS.md](USER_REQUESTS.md).

## Read first

1. `AGENTS.md` in this repo and the workspace `../AGENTS.md`.
2. [MODULE_FIDELITY_REVIEW.md](../research/modules/MODULE_FIDELITY_REVIEW.md). This
   handoff continues that review.
3. [ENGINEERING_LOG.md](ENGINEERING_LOG.md), especially:
   - request 72: uniform scaling made text unreadable, and simulated input has
     limits;
   - request 53: delivery.
4. [LICENSING.md](LICENSING.md), [MUTABLE_FIRST_SLICE.md](../research/modules/MUTABLE_FIRST_SLICE.md),
   [SPACE_DEVICES.md](../research/modules/SPACE_DEVICES.md) and
   [OPEN_SOURCE_DELAY_REVERB.md](../research/modules/OPEN_SOURCE_DELAY_REVERB.md).

## What Yan asked (request 115, verbatim)

> we still dont have any modules that look or feel like deep ones such as mimeophon,
> vhykk x or other delay/reverbs that have been logged. Also the mutable ones should
> have similar layout. IT really helps to differentiate modules and see what in/out
> options are available
>
> do a review of how much open source modules have been faithfully recreated, and
> which other modules can be approached with whats known about their design

## State at handoff

- **Repository.** `main`, pushed to origin. This handoff commit contains only this
  file and its index line.
- **Installed app.** 0.0.29 at the single Release path, delivered by another session
  on 24 November. SHA-256 `ca1d9e83…6517b50`. All 400 regular modular checks passed.
- **The review is done** (commit `80b2352`).
- **Yan has not chosen** a step from the recommended order. Yan did paste Claude's
  summary to Codex with "continue claude's work" (request 116).
- **Codex has already started step 1.** When this handoff was written, Codex's
  uncommitted 0.0.30 edits were in the working tree and were left untouched:
  - `app/Panel.h`, `app/RackEditor.h`, `app/Main.cpp`, `app/OptisynthApp.jucer`
    and `tools/make_app_project.py`;
  - `docs/project/USER_REQUESTS.md`, with request 116.

  "[Step 1 progress](#step-1-progress-in-the-working-tree)" below compares those
  edits with these notes.

### This request (117)

Yan, 26 November 2026, to Claude: "give a handoff doc to codex".

It has not been added to USER_REQUESTS.md: that file holds Codex's uncommitted
request 116, and Claude did not edit it. Codex should append it verbatim as
request 117 when it commits the log.

### Findings in brief

- **No module is a complete recreation.** Six modules wrap original MIT DSP inside
  our own controls:
  - Plaits variable-shape oscillator, sine, modal voice, resonator and analog bass
    drum;
  - the stmlib SVF;
  - plus the Clouds Parasites reverb tank inside Prism tank.

  That covers about 3 of Plaits' 24 models and 1 of the 6 Parasites modes.
- **Two modules are studies from manuals,** not code: the dual looping delay (4ms
  DLD) and the dual LPG (Optomix).
- **Every module uses the same generic card,** which is why modules look alike and
  inputs are hard to tell from outputs.
- **Mimeophon can be modelled from its detailed manual.** Vhikk X has too little
  published detail.

## What to do next

**Finish step 1 (panel descriptions)** unless Yan names another step.
- It answers both parts of the complaint: modules look alike, and their inputs and
  outputs are hard to see.
- Every later port needs it.
- It does not commit Yan to any later choice.

### Step 1: panel descriptions

#### Step 1 progress in the working tree

These are Codex's uncommitted edits as read on 26 November. They are still in
progress, so this is a checklist, not a review.

**Already there:**
- **A `Faceplate` table.** Each row has a legend, a diameter percentage and its
  parameters. It covers Modal percussion, Modal resonator, Bass drum, VA oscillator,
  Sine and Prism tank.
- **Faceplate cards put their sockets at the bottom.** Outputs move to the right
  only when all the sockets fit on one row.
- **Every output jack** sits on a filled plate, with an inverted label and an arrow.
  This applies to all modules.
- **Layout checks** cover:
  - control bounds;
  - sockets clearing the controls;
  - each faceplate control appearing exactly once;
  - a faceplate PNG for each described type, at density 44.

  The cable check now tests the content area.

**Not yet covered by those edits, compared with this handoff:**
- Generic cards still put their sockets at the top.
- When sockets wrap onto more than one row, inputs and outputs stay interleaved.
- No small (attenuverter), button, switch or lamp roles; only knob diameter
  varies.
- Dual looping delay and Dual LPG have no description.
- The footers still say "Original Plaits DSP", "Original Mutable stmlib DSP" and
  "Clouds Parasites Miverb tank". They must be renamed (item 5 below).
- Several "Done when" checks are still to be run against these edits:
  - the synth panel;
  - cable drag to the moved sockets;
  - factory preset recall.

#### Where the card is built (as of 0.0.29)

- **`app/Panel.h`, `ModulePanel`** (about line 261). In order, the card has:
  1. a header: name and family, type menu, and the `⋯` row menu;
  2. **all sockets, in rows at the top** (`resized()`, about line 456);
  3. a divider;
  4. the control grid from `controlRows(type)` (about line 236);
  5. the footer text from `panel(type).description`.
- **`Jack::paintButton`** (about line 225). Inputs and outputs are interleaved in
  port order. Outputs differ only by a faint 1 px line under the label.
- **`HardwareRack`** (about line 505) draws the cables. These all assume sockets
  near the top of the card, under a 12 px cable lane (`cableLane`, `jackPitch`):
  - `socketPosition`, `approach` and `leadPath`;
  - the clip exclusion in `paint` (about line 656).

  Moving sockets to the bottom changes this routing. The 0.0.22 behaviour ("cable
  lanes keep controls visible") must still hold.
- **Ports** come from `ModuleType{inputs, outputs}` in `src/modular/graph.h`. Each
  `Port` has a signal type and a channel count.
- **Parameters** come from `Panel` in `src/modular/runtime.h`.
- **Module definitions:**
  - Plaits and stmlib modules: `src/modular/modules.cpp`;
  - Prism tank (about line 585), the dual loop and the dual LPG:
    `src/modular/analog.cpp`.
- **Sizing.** `naturalSize()` (about line 426) feeds the rack's FlexBox, which either
  wraps or uses the rows saved in `rack.rows`.
- **Density.** Density changes the knob size (32/38/44/52/60 px). Text sizes stay
  fixed (the `text::` constants).
- **The synth panel** builds its sections separately, in `app/SynthPanel.h`.

#### What to build

1. **A panel description for each module type, in our own code.** It can sit beside
   `controlRows` or replace it.
   - **Controls.** Give each control a role: large, medium, small (an attenuverter
     next to its socket), button, switch or lamp. Allow optional grouping lines.
   - **Sockets.** Put them in an area at the bottom of the card:
     - inputs grouped on the left;
     - outputs grouped on the right, on a plate that looks clearly different
       (shape, fill or label);
     - stereo pairs side by side.
2. **Layout.** Use `juce::Grid` and `FlexBox` like the current cards. Yan has had to
   remind agents repeatedly to use existing frameworks.
   - Roles set knob diameters relative to the current density step.
   - Text stays at its fixed sizes. The 0.0.16 failure was uniform scaling that
     shrank text; do not repeat it.
3. **Modules without a description** keep the generic grid. They still get the
   bottom socket area and the output marking, because Yan wants inputs and outputs
   visible on every module.
4. **First descriptions to write:** Prism tank, Modal percussion, Modal resonator,
   Bass drum, VA oscillator, Dual looping delay and Dual LPG.
   - Where our controls correspond to the original's, follow the original
     hierarchy.
   - For example, Modal resonator could be:
     - large: transpose and structure;
     - medium: brightness and decay;
     - small: input gain.

     This follows Rings, which has Frequency and Structure large, and Brightness,
     Damping and Position medium. It is a proposal; check it against the manuals
     linked in the review.
5. **Rename the footer descriptions.** These currently say "Original Plaits DSP",
   "Original Mutable stmlib DSP" and "Clouds Parasites Miverb tank":
   - `modules.cpp`, about lines 22–28 and 76;
   - `analog.cpp`, about line 585.

   LICENSING.md says neither the Mutable Instruments name nor the original module
   names may appear on derivative works. Credit stays in
   [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the docs.
6. **No patch-format change should be needed,** because layout is per module type.
   If you add data to patches, version 6 patches must still load.

#### Done when

- **Native captures** at every density and at minimum window size show:
  - readable text at the fixed sizes;
  - nothing clipped (the "Module control text fits" check passes);
  - outputs identifiable at a glance on every card.

  Run `tools/qualify_app_ui.sh` (which uses `--snapshot`) through the launcher, and
  look at the images yourself.
- **Cables still work with the moved sockets:** drag, reroute, unplug and hover.
  - Drive this by calling the handlers directly, or with touch input.
  - JUCE hit-tests injected mouse and pen events at the real cursor position.
- **These factory presets load with the same sound and cables:** Prism afterimage,
  Optical vapour, Clay and water, and Skin and vessel.
- **The scroll wheel changes no value** on any new control.
- **`make test asan-modular` passes.**
- **A passing check is not a mature interface** (request 54). Before handing to Yan,
  use the panels as a player would:
  - patch a cable;
  - change density;
  - open the synth panel.

### Later steps (review section 4)

2. **A Beads-derived delay / granular / reverb.**
   - Before using any code:
     - add the Beads files to the queue in `tools/fetch_mutable.py`, at the pinned
       eurorack revision `f4cfa769…`;
     - read the full source and the manual.
   - Then copy the reviewed files into `third_party/mutable` and record their
     hashes in `PROVENANCE.json`.
   - Check its sample rate and buffer needs in the source. Convert 32 kHz designs
     to 48 kHz deliberately.
   - Allocate only during preparation, never on the audio thread.
   - Use our own name. Give it stereo in and out, clocking, and a panel that
     follows the original's arrangement.
3. **A complete Plaits-derived macro oscillator.** Keep the existing VA, modal and
   drum modules for older patches.
4. **Rings- and Elements-derived resonators.**
5. **Port the 4ms DLD firmware** behind the dual looping delay. The code is MIT and
   the hardware is CC BY-NC-SA. Do not use the 4ms name.
6. **A Mimeophon-style repeater** built from the manual: zones, Color, Halo, Skew,
   Flip and Hold.
7. **Vhikk X** waits for a manual or recordings.

Every port follows these rules:
- pin the revision;
- keep the MIT notices;
- use our own names;
- run offline render checks before Yan listens.

GPL code (Valley Plateau, VCV Audible Instruments) is for study only.

## Questions for Yan (one at a time)

1. If the handover did not say, confirm step 1 as the starting point.
2. Does Yan have a Mimeophon or Vhikk X, or recordings of them, for A/B comparison?
   This is only needed at steps 6 and 7.

## Rules that must keep holding

- **Requests.** Append each new request or correction verbatim to USER_REQUESTS
  when it arrives, then reconcile STATUS.
- **Workloads.** Run builds, tests, renders and browsers through
  `~/.local/bin/agent-workload`, under your own owner ID. Stop your jobs afterwards.
- **Delivery.**
  - There is one delivery path: `app/Builds/MacOSX/build/Release/Optisynth.app`,
    via `tools/build_app.sh` and `tools/deliver_app.py`.
  - Never launch, quit or replace Yan's running process.
  - `deliver_app.py` refuses delivery while the Release app is running. When it
    does, report the build as a candidate.
- **Live.** Do not drive Live.
- **Git.**
  - Make scoped commits with docs, staging explicit paths or hunks only.
  - Push, then verify the remote head.
  - Leave other sessions' uncommitted work alone.
- **Scroll wheel.** Disable JUCE wheel editing on every new control (ENGINEERING_LOG,
  request 55).
- **Copy.** In-app and docs copy should be plain and accurate. Public website copy
  is for readers; process details go to Yan and the logs.

## Other open items (not part of this work)

- **Pattern/Motion editor:** the hint line shows "Â·" instead of "·". See
  ENGINEERING_LOG, the request 88 notes. Nobody has taken it on.
- **Website** (`~/Local Sites/yanwhite/v2`, a separate repo):
  - The Optisynth page describes 0.0.21.
  - Yan has not said whether its extra spacing between title and text should apply
    to the other product pages.
  - Every deploy needs Yan's explicit approval.
