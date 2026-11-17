# Complete Spectral engine and cross-module routing

Request 32, 17 November 2026, superseded the previous immediate next step of
general polyphony work. The first whole-engine/matrix slice is implemented in
standalone 0.0.6. The original inspected plan is retained below as context.
Request 31 supplies the Pigments/Vital interaction reference.

## 0.0.6 implemented behaviour

Request 36 started this implementation. The design below remains the acceptance
reference; the following records the actual first slice, without implying the
entire older Spectral plugin editor has been ported.

- A `spectral.engine` module owns the full first-party `partials::Engine`, including
  four oscillator/filter paths, internal matrix, envelopes/LFOs, arp and effects.
  `.ptch` imports and rack-embedded patch text preserve structural settings and
  spectra; scalar controls are separate saved per-node overrides. The contextual
  panel pages expose live oscillator/filter, envelope, LFO, matrix-depth and
  matching-FX parameters. Structural spectrum/routing/arp editing remains in the
  older editor or preset file; this panel is not its full UI replacement.
- Ports declare one or two audio channels. Stereo remains separate through Runtime,
  Player and device-rate conversion. Stereo-to-mono uses an explicit average
  module; Stereo pair and Stereo mixer allow intentional stereo construction.
  Existing mono outputs duplicate exactly to both channels.
- Whole-engine loads and graph edits with reachable engines always use the single
  preparation worker, including cached loads. Compatible tables are immutable
  resources reused by the engine's exact-content check. Dormant nodes have no
  processor and their table cache is released. At most two full engines may be
  active in one plan; crossfades/worker/pending plans can coexist, so this is not
  a process-wide memory cap. Cancellation discards stale results; an in-progress
  core table build finishes before that worker can take the newest request.
- The shared matrix stores instance-qualified control-output/parameter routes.
  It resolves dependencies and indices before audio starts, includes control-only
  sources in reachability and rejects cycles, invalid targets and audio sources.
  Depth changes use atomics; structural route changes prepare a new graph.
  Summed signed offsets act on a normalised parameter range and clamp to bounds;
  positive logarithmic controls map offsets exponentially. Source values clamp
  to -1..1. Updates occur once per render chunk, at most 24 internal samples;
  this is control-rate modulation, not audio-rate or new sample-accurate events.
  Stored knob values remain unchanged by modulation.
- A separate sine/triangle/ramp/pulse LFO can modulate eligible parameters.
  Spectral exposes actual LFO1–5, main, amp1–4, cut1–4 and mod1–3 envelopes.
  Read-only snapshots select the newest-started active voice, including release;
  silence gives zero. Downstream modules receive the end-of-chunk snapshot held
  for that chunk. The internal engine's own modulation path is unchanged.
- Destination filtering uses saved structural state: disabled oscillator/filter,
  absent effects, unused internal modulation sources and identical wave mixes
  are omitted where identifiable. This is conservative metadata, not a measured
  guarantee that every setting/stage has audible impact. Self-assignment through
  the external graph is rejected; Spectral's internal matrix handles those routes.

Rack format v3 embeds full engine state and parameter assignments, retaining v1/v2
and legacy instrument import. Spectral text is bounded to 1 MiB total during rack
read and external preset import to 1 MiB/file; rack files to 2 MiB. Ordinary
parameter state is recalled, not in-flight voices or effect-buffer contents.
Structural replacement uses the existing 5 ms plan crossfade and retriggers the
held shared keyboard note. Long-tail transfer and polyphonic event cables remain
pending. No VST3 installation, DAW control or external code adoption is involved.

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

## Code evidence inspected before implementation

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

## Original implementation order and acceptance

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

The above adapter, stereo/state schemas and first matrix are now implemented.
Qualification: 151 modular checks normally and under ASan/UBSan, 1,194 core checks,
28 graph checks, importer/renderer compatibility and native state/UI checks pass.
Direct/wrapped stereo audio matches exactly for simple and filtered/unison/arp/FX
fixtures; tested render/converter paths allocate nothing. Evidence and limited
performance measurements are recorded in STATUS and spectral_build_receipt.json.
Dense polyphony, whole-engine transition peaks and listening remain further work.

Next: define sample-timestamped events and transport for the independent sequencer
and arp, then implement the first step model. Keep structural Spectral editing,
modulation indicators/direct assignment and broader effect modules on the queue.
