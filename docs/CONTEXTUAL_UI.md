# Contextual hardware-style rack — 17 November 2026

Request 25 follows successful user audition of the Mutable rack. Redesign the
existing native JUCE instrument; preserve its DSP, patch format and routing engine.
Sample playback remains queued behind this interface step.

## Visual references inspected

- [Moog Matriarch top-panel photograph](https://www.altomusic.com/cdn/shop/files/d1948-top.jpg?v=1702400243), local ignored `references/matriarch-panel.jpg`.
  Adjacent functional sections, blue oscillator block, green filter, dark mixer/
  envelopes/utilities, purple delay, black/silver knobs, pale labels and patch points.
- [Original Plaits manual/panel](https://pichenettes.github.io/mutable-instruments-documentation/modules/plaits/manual/), local `references/mutable_review/plaits-panel.png`.
  Clear control hierarchy, distinct pitch/tone controls and labelled input/output
  sockets. Our modules expose individual original DSP algorithms, not the entire
  Plaits instrument, so retain accurate controls rather than inventing absent modes.
- Current 0.0.3 native screenshots in `build/ui-review/` establish the working UI
  and controls. Product Design context preflight found no saved context. Apply its
  visual review workflow to the native app; web scaffolding/assets are inapplicable.

## Implementation direction

Show multiple loaded modules side by side in a horizontally scrollable rack, with
contextual panels and readable controls. Keep a focus view for larger editing.
Oscillator tuning/tone hierarchy, a dominant filter cutoff knob, envelope stage
layout and a resonator-specific control grouping replace the generic equal grid.
Use functional native knobs/sliders/buttons and typography, with our own panel
styling. Source photographs are reference only, not app artwork or copied branding.

Patch points should operate the existing typed routes. Preserve clear selected-node
feedback, dormant status, output selection and validation messages. All existing
save/recall, typing/piano, audio start/stop and independent instance bindings remain.

Acceptance: native screenshots at normal/minimum size and several module types;
visible controls fit; loading/replacing a module changes its panel; multiple copies
remain independent; routing and patch recall still pass native checks. Compare the
reference and native result together, documenting intentional software differences.

## Implemented behaviour

Standalone 0.0.4 shows side-by-side panels, horizontal scrolling for larger racks,
and a Focus module switch for the enlarged panel and connection list. Each panel's
selector changes its type; the existing graph validator rejects incompatible edits.
Add module creates a dormant independent instance. Colours follow module families;
layout follows the module's actual controls. Filter cutoff, envelope stages, mixer
faders, oscillator shaping and resonator material each have distinct arrangements.

Click an input socket to connect/disconnect a same-type output. Click an output to
choose a destination or make an audio port the instrument output. Tooltips list
existing routes; coloured centres mark connected ports. This step uses native
route menus; draggable cables remain pending. Invalid edits preserve the live rack.
Scalar edits synchronize rack/focus views and saved patch parameters without
rebuilding DSP. Module replacement is deferred out of the source selector callback
before rebuilding the visible rack, avoiding deletion of a widget handling an event.

The sound engine, parameter identities/ranges and patch format are unchanged.
Visual review evidence and limitations are in `../design-qa.md`.
# Modulation UI option — request 31, 17 November 2026

Yan supplied a Pigments screenshot and suggested the Pigments/Vital modulation
matrix style. Consider a persistent coloured source strip, selection/dragging of a
source onto a parameter, a depth indication around the knob, and a matrix listing
source, destination, amount and polarity. These are proposed interactions, not
implemented features or a claim about either product's internals.

Use the same stable module-instance/parameter IDs for direct assignment and the
matrix. Keep audio paths and module socket connections legible. A shared modulation
representation should support both views rather than duplicate routing state.
Expose useful destinations on active modules, consistent with Yan's earlier
request about saved-preset destinations. Shared versus per-voice sources, smoothing,
audio-rate modulation and signal scaling still need explicit engine contracts.
Finish the sample-player milestone first; research the original manuals before
implementing this interaction. The existing hardware-panel direction remains.

## First matrix and complete engine — 0.0.6

Matrix now opens a shared source/destination/depth editor with add/remove and live
depth controls. Routes use saved instance/parameter IDs and graph validation.
Spectral destination choices use effective preset state. The matrix does not yet
provide drag-to-knob assignment, coloured source strips or depth rings.
Modulation scope/scaling is defined in `SPECTRAL_ENGINE_INTEGRATION.md`.

The complete Spectral panel has stereo audio plus 17 actual LFO/envelope outputs,
preset loading, a section selector and six controls per page. Oscillator/filter,
envelope, LFO, internal matrix depths and matching effects have separate sections.
Structural spectrum/arp/routing editing is still supplied by preset state or the
older editor. At minimum size, six knobs occupy one readable row; taller views
use two rows. Independent LFO has named shapes and a note-reset switch. Explicit
stereo adapters explain their operation on the panel. Native captures and limits
are recorded in `../design-qa.md`.
