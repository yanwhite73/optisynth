# VCV performance and modularity lessons

Requests 28–29, 17 November 2026. Primary documentation and selected source paths
inspected read-only. No VCV code installed, executed, built or adopted. This is an
architecture recommendation; no comparative benchmark or new implementation claimed.

## Performance evidence

- [Polyphony manual](https://vcvrack.com/manual/Polyphony): multichannel cables and
  SIMD can make a polyphonic module more efficient than duplicated mono modules.
  Its speedup examples do not predict the performance of our Mutable adapters.
- [Fundamental VCO](https://github.com/VCVRack/Fundamental/blob/ce3326407eb49ba188509b07912d58772b22e284/src/VCO.cpp#L636):
  four-lane processors, channel iteration in groups of four, connected-output
  waveform enables, and divided light updates. SIMD is distinct from multithreading.
  Necessary oscillator phase/state still advances.
- [Rack engine](https://github.com/VCVRack/Rack/blob/061ccf63c1758599396ac1bb10d47345d9d34076/src/engine/Engine.cpp#L307):
  dispatch registered modules per sample, synchronize workers, then transfer cables.
  This implies one-sample inter-module cable delay. The dispatch loop does not prune
  by output reachability, although individual modules can bypass/skip work. Block
  entry takes locks. It is not evidence of a universally lock-free engine.
- [Engine settings](https://vcvrack.com/manual/MenuBar#threads): start with one thread;
  more threads can support larger racks while increasing total CPU consumption.
  Sample-rate choices and optional CPU meters also carry a cost.
- [Developer guide](https://vcvrack.com/manual/PluginGuide#framebuffer): cache complex
  static graphics and redraw when dirty. Its [asset guidance](https://vcvrack.com/manual/PluginGuide#patch-storage)
  recommends separate storage for large patch data and avoiding file I/O in DSP.
- [Module implementation](https://github.com/VCVRack/Rack/blob/061ccf63c1758599396ac1bb10d47345d9d34076/src/engine/Module.cpp):
  explicit bypass routes and sampled performance-meter machinery. Instrumentation
  belongs behind a switch and its overhead must be measured too.

## Optisynth consequences — recommendations

Retain the compiled ordered block graph and resolved buffer pointers. This avoids
a per-sample cable-copy pass; that is a plausible efficiency benefit, not a measured
claim that Optisynth outperforms Rack. Adding cycles requires defined feedback delay
semantics; a 24-frame cycle changes sound differently from one sample of delay.

Design future voice state for batch/SIMD processing on Apple Silicon. Share immutable
sample/table data, and run explicitly global FX after voice mixing. Per-voice filter,
envelope and playback state remain independent. A vectorized wrapper will not
automatically vectorize the original Mutable algorithms inside it.

Begin with a good single audio-thread path. Test many VST instances in a host before
adding per-instance worker pools. If parallel work is justified, benchmark coarse
voice/layer jobs, synchronization overhead and worst callback time. Keep local
oversampling and rate conversion explicit; do not raise the whole graph's sample
rate as a blanket response to aliasing. Audio-rate modulation and gate timing must
remain correct when slower display/control tasks are decimated.

Use static contextual panels with independent dynamic controls/meters/cables.
Sample decoding, graph preparation and asset persistence stay off audio processing.
Bound decoded assets and loader queues; use compact patch metadata and define
missing-asset/cancellation behaviour. A silent module may still have important state
or tails. A module outside all active dependencies should not load DSP/assets simply
because its panel is visible.

## How modular is the current implementation?

Inspected Optisynth source at `184618a`:

| Area | Current implementation | Proposed next boundary |
| --- | --- | --- |
| DSP instances | Separate module factories, parameters and state; independent copies | Retain stable instance identities |
| Audio routing | Explicit buffers connect sources, filters, VCA, mixer, resonator | Add useful audio ports, stereo and voice/global buses |
| Control routing | Envelope output fans out; filters expose modulation and VCA exposes gain | Explicit modulation destinations with units, depth and polarity |
| Notes and gates | The same `Note` context reaches every active module | Voice groups with default note routing and explicit overrides |
| Oscillator pitch/shape, resonator material | Scalar parameters; no general modulation inputs | Per-module modulation capabilities, including audio-rate where supported |
| Filter modulation | Reads the first modulation sample of each <=24-frame block | State the control bandwidth; implement faster paths when needed |
| Events | Graph enum exists; runtime rejects event cables | Sample-offset note/gate/trigger/clock routing |
| Polyphony | One monophonic note context | Bounded voice allocator, clear global/per-voice ownership |
| Feedback | Cycles rejected, even in dormant graphs | Explicit causal feedback implementation and tests |
| Extension | Module registry compiled into the app | Developer adapter contract; dynamic third-party hosting is a separate choice |

Sources: `src/modular/modules.cpp` registry/adapters, `runtime.h/.cpp`, `graph.cpp`,
`rack.h` and `player.h`. The Mutable entries expose selected original algorithms;
they do not yet expose every control, mode or patch socket of the complete hardware
modules. ModalVoice also contains its own exciter/resonator structure; a separate
externally excited resonator is already available.

VCV's [patching model](https://vcvrack.com/manual/GettingStarted) connects voltage
outputs to inputs; its polyphonic cables carry multiple channels. Optisynth currently
uses distinct audio/control/event types and explicit mixers. Copying the visual
cables alone does not reproduce those signal semantics.

## Recommended environment structure

Use a modular DSP core and patch document with playable instrument groups:

1. Modules expose their meaningful audio, modulation and event interfaces, including
   units/rates, valid ranges and lifecycle behaviour.
2. A **voice group** contains an editable module graph. Notes create voices through
   one allocator; default pitch/gate wiring makes a loaded instrument playable.
   Defaults must be visible/inspectable in the patch description. Explicit routing
   can override them without requiring every ordinary preset to be wired manually.
3. A **rack/global layer** combines instruments with shared effects, modulation,
   mixing and transport. One global LFO differs from a retriggered LFO per voice.
4. Templates instantiate these groups; macros expose useful parameters. The full
   editor and future agent API address the same module/port/parameter IDs.

This provides reusable internals and composing convenience. Groups are proposed
schema/engine work, not implemented container widgets. The agent should be able to
inspect effective routing, scopes and saved values without simulating UI actions.
Typed units and explicit conversions can support unusual audio-as-modulation routes
when the receiving DSP supports them; avoid an arbitrary restriction to slow LFOs.

## Current performance foundations and qualification gaps

`graph.cpp` prepares reachable dependencies only, including control sources.
`runtime.cpp` resolves pointers before rendering and runs <=24-frame blocks.
`player.h` exchanges prepared graphs and retires them off audio. Its 240-sample
crossfade briefly processes two graphs, so measure transition CPU and memory peaks.
The output converter also consumes CPU. `RackEditor.h`'s 60 Hz timer repaints only
the small output-meter area; preserve that restraint when adding cable animation.

Proposed benchmark matrix, as capabilities arrive:

- Graphs: current starters, long chains, fan-out, many dormant modules, resonator.
- Voices: 1/4/8/16, sustained notes, attacks, release tails and voice stealing.
- Device rate/block: 44.1/48/96 kHz and 32/64/128/256 frames.
- UI: hidden, static, dragging controls/cables and scrolling; meters on/off.
- Changes: rapid graph swaps, sample loading/cancellation, failed edits/assets.
- Host: one/many future plugin instances; offline and realtime qualification separate.

Record callback duration distributions/maxima, missed deadlines/dropouts, CPU,
memory/preparation peaks and sound/timing regression. At 48 kHz, 64 frames represent
about 1.33 ms; this arithmetic is not a promised utilization target. Existing
small-rack average throughput does not establish maximum polyphony or VST readiness.
No new benchmarks run here. The earlier sound-quality/aliasing deferral remains.

## Sequence and evidence record

Before expanding sample/polyphony architecture, define module capabilities,
modulation/event ports and voice/global ownership. Then implement immutable sample
assets/loading/playback, voice allocation and shared FX; fit direct cable/undo UI
work to the same graph. Keep shipping small playable milestones; optimization follows
measurements. Requests 28–29 steer this design work and do not cancel sample playback.

GitHub API branch metadata checked 17 November 2026:

- Rack v2: `061ccf63c1758599396ac1bb10d47345d9d34076`, 4 November 2025.
- Fundamental v2: `ce3326407eb49ba188509b07912d58772b22e284`, 8 November 2026.

Manuals are live documents accessed on the research date. Rack's
[SIMD common header](https://github.com/VCVRack/Rack/blob/061ccf63c1758599396ac1bb10d47345d9d34076/include/simd/common.hpp)
selects native SSE or SIMDe. Portable vectorization is the lesson; no unreviewed
x86 implementation is assumed compatible with our build. A full dependency/licence
review would be needed before adopting any VCV code.
