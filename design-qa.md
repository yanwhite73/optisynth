# Contextual rack UI review — 0.0.5 (retaining the 0.0.4 review)

Source visual truth: `docs/references/matriarch-panel.jpg` (1920×1920 photograph),
plus `docs/references/mutable_review/plaits-panel.png` (324×604 manual panel).
The Matriarch instrument occupies roughly x=10–1910, y=530–1390. Compare its panel
organisation and control hierarchy, not the white photograph margins or branding.
This is a native software adaptation of hardware references, not a pixel replica.

Implementation: `build/ui-review/hardware-*.png`, native JUCE component captures
at 1440×940 and 1120×800, density 1. No CSS/browser viewport applies. All captures
run with the audio device closed. The reference and implementation images were
opened together in one comparison input, with focus and dense-rack views inspected
separately for readable details.

## Comparison history

First pass: 17 November 2026. Default oscillator/filter rack, external resonator,
minimum-size AHDSFR, enlarged filter, and two-oscillator/mixer states inspected.

- P2: At 1120×800 the AHDSFR attack/hold/decay/release knobs collapse to tiny dots.
  Give these four knobs two taller rows, place fade below sustain, keep curve trims
  along the bottom. Check the denser Spectral filter at the same minimum size too.
- P2: Sustain and mixer use thin generic slider tracks/dots, losing the reference's
  clear fader handles. Draw rectangular handles, centre lines and vertical ticks.
- Source/reference grouping is otherwise present: blue oscillators, green filters,
  dark envelopes/utilities, purple resonators; independent sections and top sockets.

Second pass: envelope/fader fixes confirmed in revised minimum and dense-rack
captures. The new Spectral minimum capture exposed small VA/ADSR controls too (P2).
Compact rotary controls now put their values beside the knob, recovering knob area.

Third pass: rebuilt and recaptured the same states. Opened Matriarch, both minimum
captures, original Plaits panel and final resonator capture together. Larger compact
knobs, stage labels, values, fader handles and trim rows are visible and separate.
Normal-size rack/focus proportions are preserved. No actionable P0/P1/P2 remains.
Current six `hardware-*.png` files are the final captures; earlier iterations were
inspected live and superseded at these paths.

## Required surfaces

- Typography: native sans serif, uppercase compact control labels, larger module
  names, editable numerical values. Exact hardware lettering is intentionally not
  copied. Enlarged focus view supports reading/control without a wider rack.
- Spacing: adjacent panels and shared socket baseline; per-module control hierarchy.
  Horizontal scroll handles large racks while global controls remain visible.
  Minimum envelope density corrected and verified at 1120×800. Compact values move
  beside their knobs; larger views retain values below.
- Colours: muted blue/green/purple with dark neutral utilities and pale labels;
  yellow selection outline and control connection dots distinguish selection/routing.
- Images: no photo skin or logos in the app. Controls are functional native widgets;
  the reference is a physical instrument, not image artwork being ported to software.
- Copy: accurate algorithm/parameter names; no absent Plaits modes or Matriarch DSP
  claims. Unrouted modules explicitly show their state. Socket tooltips show routes.

## Interaction evidence and limits

Native checks cover independent copies, rack/focus value synchronization, typed
connections, invalid graph preservation, patch/audio recall and legacy import.
Added checks cover socket action handlers, route indicators/tooltips, contextual
replacement and all 17 panel bounds. All pass on the final native run. Menus use native JUCE
PopupMenu; pointer-driven menu selection is not claimed as automated coverage.
Sockets open route menus; drag cables and audio-rate parameter patching are pending.
User audition of 0.0.3 passed; redesigned 0.0.4 audition is pending.

Full-view evidence: `hardware-rack.png`, `hardware-resonator.png`,
`hardware-two-oscillators.png` (1440×940). Focus evidence: `hardware-filter-focus.png`
(1440×940); dense-control evidence: `hardware-minimum.png` and
`hardware-spectral-minimum.png` (1120×800). Controls/labels are readable in these
1× images, so a separate enlarged crop is unnecessary. Source and rendered density
differ intentionally because this is hardware-to-software adaptation; no pixel
equality is claimed. Native release log has no build errors; browser console N/A.

Follow-up polish (P3): richer knob surface shading and cable overlays. These are
not required for the functional contextual-panel milestone.

## Sample-player extension — 0.0.5

Compared Matriarch reference, new sample/resonator normal view and 1120x800 sample
view together. Gold sample panel retains the established muted hardware palette;
top output socket, file/loop row, filename, waveform, three main controls and trim
faders have separate readable areas. The parallel mixer view follows the same
panel hierarchy. All 18 panels pass minimum-size bounds and overlap checks.

First visual pass found P2 garbled ellipsis text on Load WAV (UTF-8 bytes passed to
a narrow JUCE string constructor). Replaced new narrow UI literals with ASCII and
clarified root-note value as MIDI. Rebuilt and reran native checks; final sample
minimum, parallel and original VA rack captures were opened together. Button copy
is readable, loop state is visible, controls/values remain separate and the original
rack is preserved. No actionable P0/P1/P2 remains.

New captures: `build/ui-review/sample-resonator.png`, `sample-parallel.png` at
1440x940; `sample-minimum.png` at 1120x800. The latter shows a loaded external WAV,
nonzero start trim and held looping. Six previous captures are regenerated too.
Native checks additionally cover loaded waveform ownership, loop/focus sync,
external WAV recall audio, failed-load preservation and background cancellation.
No audio device was opened. Actual Finder dragging and audio audition are pending.

Request 31's Pigments/Vital suggestion is a future modulation interaction option;
this review does not claim that a modulation matrix has been implemented.

final result: passed

## Complete Spectral and shared matrix — 0.0.6

Continued the approved hardware-panel direction. Inspected native Spectral/stereo
and Spectral/resonator captures at 1440x940, Spectral controls at 1120x800 and the
matrix at 960x560. The first pass found P2 tiny knobs when twelve controls shared
the shorter panel. Six controls per page, arranged in one or two rows according
to available height, restore readable knobs, labels and values. Page count and
section selection remain visible above the controls.

Follow-up review caught stale matrix evidence: its stream appended a second PNG
instead of replacing the first. Added truncation, rebuilt and recaptured; the
matrix now visibly shows the current friendly parameter label and tested 0.230
depth. The initial LFO shape value required an explicit text refresh to display
Sine rather than 0. Both corrections are visible in the final images.

Exported Spectral LFO rates must remain visibly active when routed outside the
engine. Updated panel eligibility and socket indicators/tooltips to include
matrix assignments; the final resonator capture shows the lit LFO1 socket and
undimmed rate. The native check asserts both. Matrix source/destination selectors,
depth controls and Remove are readable with separate hit regions.

Final files: `build/spectral-ui-review/spectral-stereo.png`,
`spectral-resonator.png`, `spectral-minimum.png` and `spectral-matrix.png`.
Nine existing hardware/sample captures are regenerated there too. All 23 panels
pass bounds/overlap checks, and native state/load/modulation checks pass on the
final build. No audio device is opened by captures. Physical pointer audition and
listening are pending. Drag-to-parameter assignment, modulation rings and cable
overlays remain later work; this first matrix uses explicit selections.

No actionable P0/P1/P2 remains in the reviewed surfaces. Final result: passed.

## Sequencers — standalone 0.0.7, 19 November 2026

Request 43 uses the collected Bespoke/PER|FORMER/OXI workflow research. Pattern
has selectable note cells and a scoped pitch/velocity/rest inspector. Stage uses
duration-weighted blocks and a pitch/pulses/articulation inspector. Euclidean
shows generated hits around a ring with length/hits/rotation controls, rather
than pretending its cells are stored notes. A pale outline indicates selection;
a gold marker indicates the actual audio playhead. Arrow keys change selection;
double/right click toggles Pattern rests. Numeric editors retain keyboard-focus
protection from Caps Lock note entry.

Native checks exercise the real controls, stored parameters and runtime handles;
saved documents reproduce the same offline audio and tempo. A new assertion
requires stop to clear the displayed playhead without waiting for another audio
callback. The first small-window capture showed a cramped Euclidean ring; moving
its inspector to the right made all positions distinguishable. Stage short cells
now have a readable minimum width; additional width reflects pulse duration.

Final captures: `build/sequencer-ui-review/sequencer-{0,1,2}-{focus,rack}.png`
and `sequencer-minimum.png`. Checked at the 1120 x 800 minimum. Existing rack,
sample, Spectral and matrix captures/checks also pass. Audio remains off during
native checks; visible playheads come from explicit offline Player rendering.
Listening and physical pointer audition remain user checks. General undo,
multi-cell selection, chord/piano-roll modes, arranger and drawn cables remain
future work. Source and binary hashes are in the sequencer build receipt.
