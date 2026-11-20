# Rack zoom, synth-panel view and preset browser

Request 71, 20 November 2026. This note was written before implementation. It
records the mechanism, the file format and the success criteria. The results
section at the end is filled in after the native checks.

## What changes

### 1. Header

The three loose button rows become three bands:
- **Preset bar:** previous / next, favourite star, the current preset name with
  its category and tags (click to browse), and Save.
- **View switch:** Panel | Modular, plus a menu for the view preference.
- **Transport:** play, restart, tempo, position and DSP load. The status message
  sits on the same band.
- **Rack tools** (modular view only): add / remove module, instrument output,
  matrix, focus and zoom.

### 2. Rack zoom

- The rack sits inside a zoom host in the viewport, and the host scales it with
  a component transform. Text and knobs are drawn as vectors, so they stay sharp
  at any zoom. Mouse hits and popup positions follow the transform.
- Range is 50–200%, using −, + and Fit buttons, ⌘− / ⌘= / ⌘0 and trackpad
  pinch. The scroll wheel still only scrolls (request 55).
- Modules wrap into rows at the zoomed width. Zooming out therefore shows more
  modules at once, instead of shrinking one long row.
- **Fit** picks the largest zoom at which the whole rack is visible without
  scrolling.
- Zoom is a per-user preference. It is not stored in the patch.

### 3. Synth-panel view

- A fixed front panel of the patch's playing controls, with no sockets or
  module selectors. It is built from the same parameters as the rack, so
  editing either view moves the same values, the same audio-thread handles and
  the other view's knob.
- **Layout source:**
  1. **Curated:** the patch carries a `panel` with titled sections. Each section
     lists `{node, parameter, label}`. The factory pads have curated panels:
     oscillators, mixer, poly-mod, filter, both envelopes, voices, modulation
     and effects.
  2. **Automatic,** when the patch has no layout: one section per module, in
     rack order.
     - Modules without parameters are skipped.
     - Large modules show a reduced set: the Spectral engine shows its main page.
     - Sequencers get a button that opens their editor.
     - Mixer inputs are labelled with the module that feeds them.
- A layout control whose module or parameter no longer exists is dropped when
  the patch changes, for example after replacing a module.
- **Styles** are original colour schemes: graphite, slate, walnut and ivory.
  None copies a manufacturer's panel.
- Sections wrap into rows. The knob size is the largest that fits the window,
  between 60 and 96 px. Below that minimum, the panel scrolls vertically.

**Which view opens.** The patch stores `view` (`panel` or `modular`).
- The preference "Open patches in" offers: patch's saved view (default), always
  modular, or always panel.
- Switching view by hand updates the patch's `view`, so the next save keeps it.

### 4. Preset browser

An overlay over the rack/panel area. It opens from the preset name or Browse and
closes with Esc.
- **Left:** collections — All, Favourites, Factory, User — then categories, each
  with counts.
- **Top:** a search box covering name, category, tags and description, and tag
  chips. Selected tags must all match.
- **Middle:** a sortable table with columns ★, Name, Category, Tags and Source.
  - Click selects a preset. Double-click or Return loads it.
  - Clicking the ★ cell toggles favourite.
  - Up/down arrows move the selection.
- **Right:** details of the selected preset.
  - Factory presets are read-only; save a copy to change their tags.
  - User presets have editable name, category, tags and description, written
    back into the file.
  - "Save current sound" uses the same fields and writes into the library.
- **Footer:**
  - library folder;
  - Add folder… (for example the repository's `patches/`);
  - Reveal;
  - Import file…;
  - Keep current sequence.

**Categories:** Pad, Keys, Lead, Bass, Percussion, Sequence, Texture, Drone,
Template. **Tags** are free text, lower-case, up to 16 per preset.

Previous / next step through the browser's current filtered and sorted list.

## Storage

| What | Where | Why |
| --- | --- | --- |
| Category, tags, description, name | Inside the `.optisynth` file (`info`) | Travels with the patch |
| Preferred view and curated panel | Inside the file (`view`, `panel`) | "Dependent on patch" |
| Favourites, zoom, view preference, extra library folders, Keep sequence | `~/Library/Application Support/Optisynth/Optisynth.settings` | Per user, not per patch |
| User library | `~/Music/Optisynth/Presets/` (created on first save) | Ordinary files the user can back up |

**Patch format** version 6 adds three optional fields: `info`, `view` and
`panel`. Versions 1–5 still load, with empty metadata.

Editing a user preset's metadata changes only those fields:
1. Parse the JSON.
2. Replace `info`.
3. Run the full patch validation.
4. Write to a temporary sibling, then replace the file.

The rest of the file is preserved as written.

**Native checks** never touch the real settings or library. Snapshot mode puts
both inside the snapshot directory.

## Success criteria (native snapshot checks)

1. **Factory catalogue.** It lists all 21 starters. Each has a valid category and
   at least one tag, and loading by catalogue gives the same audio as before.
2. **Filtering.**
   - Category, tag, search and favourites filters return the expected presets.
   - Favourites persist in the settings file across a reload.
3. **Saving and editing.**
   - Saving to the library writes a version 6 file, and scanning finds it with
     its metadata.
   - Editing its tags keeps it loadable and its audio identical.
4. **Previous / next** follow the filtered list and wrap at its ends.
5. **Synth-panel controls.**
   - Panel → DSP handle and rack card.
   - Rack card → panel.
   - Panel edits are saved in the patch.
6. **Views.**
   - Pads open in the panel view under "patch's view".
   - "Always modular" overrides that.
   - Manual switching is saved with the patch.
7. **Panel layout.**
   - Automatic layout covers non-curated patches.
   - Panel controls stay in bounds without overlap at the default and minimum
     window sizes.
8. **Zoom.**
   - At Fit, the 12-module pad rack is visible without scrolling.
   - At 150%, the host is larger than the viewport.
   - Selecting a module scrolls it into view at any zoom.
9. **Format.**
   - Version 6 round-trips info, view and panel.
   - A version 5 file still loads.
   - A panel referencing a missing parameter is rejected on read.
   - Replacing a module prunes stale panel controls.
10. **Captures.** The four pad panels, an automatic panel, the browser, the save
    form, minimum-size panel and browser, and the rack at 60% and 150%. Each is
    inspected before delivery.

## Results (20 November 2026, 0.0.16)

All ten criteria pass in the native checks (`panel-browser-check.txt`), on the
delivered binary. The existing UI, sequencer and pad checks also still pass.

**Measured behaviour:**
- The 12-module OB swell rack fits at 65% at the default window size, and at
  50% at the minimum size. Both show two rows.
- At 150% the rack scrolls both ways, and selecting a module scrolls it into
  view.
- The curated pad panels have 47 to 53 controls in 9–10 sections. They fit
  without scrolling at the default size, and scroll vertically at the minimum
  size.

**Captures** in `build/sequencer-ui-review/`, all inspected:
- `panel-ob-swell.png`, `panel-prophet.png`, `panel-xpander.png` and
  `panel-drone.png`;
- `panel-automatic.png` and `panel-minimum.png`;
- `browser.png`, `browser-save.png` and `browser-minimum.png`;
- `rack-zoom-fit.png`, `rack-zoom-60.png`, `rack-zoom-150.png` and
  `rack-zoom-fit-minimum.png`.

**Fixed after the first inspection:**
- The Panel / Modular switch did not show which view was active.
- Slow LFO rates read "0.0 Hz". Hz values below 1 now show three decimals,
  for example 0.030 Hz.
- The preset table showed a needless horizontal scrollbar.
- Spare panel height collected at the bottom. Sections now grow up to 35% taller
  to use it.

**Not done yet:**
- **Editing a panel layout by hand,** for example right-click → "show on panel".
  Today a patch either carries a curated layout or gets the automatic one.
- **Modulation-route depths as panel controls,** such as wheel → cutoff.
- **Pinch** is exercised through the same handler the trackpad uses. A real
  gesture has not been checked by hand.
- **The minimum window size** needs a vertical scroll for a full pad panel.
- **Untested by a person:** keyboard browsing in the table, the overwrite
  dialog, Add folder and Reveal. None of these was clicked through by a person.
