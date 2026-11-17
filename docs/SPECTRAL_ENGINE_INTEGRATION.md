# Complete Spectral engine and cross-module routing

Request 32, 17 November 2026, supersedes the previous immediate next step of
general polyphony work. This is an inspected integration plan, not a claim that
the whole engine or an environment-wide modulation matrix is already available
in standalone 0.0.5. Request 31 supplies the Pigments/Vital interaction reference.

## Intended result

Load Spectral as one complete instrument block, preserving its four oscillator/
filter paths, envelopes, five LFOs, internal matrix, arpeggiator and effect routing.
Keep whole-preset state and a contextual engine editor. Existing extracted Spectral
filter/envelope modules remain optional building blocks; splitting the complete
engine into independent modules is not this task.

The environment's matrix should connect Spectral with other modules: route its
audio through a filter/resonator or mixer; allow another modulator to move an
eligible Spectral parameter; expose selected actual Spectral modulation signals
for external destinations. Matrix and direct assignment must share one saved
representation. Preserve internal matrix rows when adding external assignments.

## Code evidence inspected

- `src/core/engine.h/.cpp`: `partials::Engine` already owns complete instrument
  playback. `load()` builds tables/effects off audio, reuses compatible `TableCache`,
  and resolves the internal matrix. Note/controller/render operations are bounded
  realtime paths. `render(left,right,frames)` is stereo; `setLiveControls()` accepts
  the existing live parameter array. Live-control mode fixes oversampling so host
  modulation can activate XMod/resonance without changing the processing rate.
- `src/core/live_controls.h`: stable parameter IDs, ranges, patch keys and scalar
  application already exist for oscillator/filter controls, LFO rates, envelopes,
  matrix amounts and FX. `liveControlActive()` only performs a limited FX type
  check; it is insufficient as a complete audible-destination eligibility policy.
  Preserve Yan's requirement to avoid useless assignments for saved presets.
- `src/core/patch.h`: complete `Patch`, `patchText()` and `parsePatch()` provide
  embedded state serialization. Synth-internal `MatrixSlot` has stable source/
  destination strings and depth; the environment needs instance-qualified IDs.
- `Voice::sourceValue()`/`runModulation()` and private voice state contain the actual
  LFO/envelope signals. There is no public export API. Duplicating an approximate
  external LFO would not expose the oscillator's real modulation phase/value.
- `plugin/Source/PluginProcessor.*`: existing plugin adaptation uses a background
  loader and engine lock. Reuse the core engine/state/schema, while keeping the
  modular app's prepared-plan exchange. Do not import a second device/host lifecycle
  or the plugin's lock into the modular audio callback.
- `plugin/Source/PluginEditor.cpp`: existing full editor is coupled to
  `PartialsProcessor`, APVTS and patch revisions. A small editor/controller adapter
  is needed; instantiating the existing processor just to obtain its editor would
  duplicate ownership and loading paths.
- `src/modular/runtime.*`, `player.h`, `graph.*`: current execution and output
  conversion are mono; all modules share one Note. Graph edges target ports, not
  parameters. Cycles are rejected and dormant nodes are pruned. Stereo and
  parameter-route dependencies need explicit extensions, not hidden UI wiring.

## Implementation order and acceptance

1. Define a complete-engine state/resource record outside GraphSpec, with embedded
   patch text and immutable reusable tables. Generalize preparation so every
   reachable Spectral engine builds off audio and expensive graph changes never
   fall through the current cached UI-thread preparation path. Dormant engines
   must not construct tables or effect buffers.
2. Extend channel-aware routing, Player and output conversion to preserve stereo
   end to end. Existing mono patches should retain identical samples in both
   channels. Stereo-to-mono conversion for mono modules must be explicit. Compare
   a direct full-engine render with the wrapped engine's L/R outputs using the
   same rate, live-control mode, note sequence and patch state.
3. Add a stable parameter-destination route representation: source instance/port,
   destination instance/parameter, amount and mapping units. Keep stored base
   values separate from modulation. Resolve handles and dependency order before
   rendering; account for control-only reachability and cycle checks. State a
   control rate/scaling contract; do not label block-rate routes audio-rate.
4. Expose actual internal modulation outputs with explicit voice ownership. A
   Spectral patch may have overlapping releases or arp/polyphonic notes even if
   the surrounding keyboard is mono. Decide and document voice selection or
   aggregation before exporting a single LFO/envelope value to a global module.
   Preserve internal synthesis and routes with exports disconnected.
5. Add the complete-engine panel/preset load/editor adapter and shared matrix UI.
   The parameter list should reflect enabled/routed saved-preset destinations.
   Demonstrate external envelope/LFO to Spectral, Spectral modulation to another
   module, and stereo Spectral audio through the environment.

Verify full-patch recall, independent engine instances, disconnected-route baseline
audio, failed/cancelled preparation, bounded memory and zero callback allocation/
locks. Benchmark cold loads and graph-transition peaks as well as steady playback.
No new VST3 packaging or Live operation is implied by this standalone milestone.

Exact next implementation action: specify the channel-aware port/output and
embedded whole-engine state schemas, then implement a stereo-preserving core
adapter and qualification render before adding the matrix UI. This document
records the reason stereo and modulation scope cannot be skipped.
