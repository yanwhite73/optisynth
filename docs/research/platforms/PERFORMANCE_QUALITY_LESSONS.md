# Performance, quality and threading: lessons from Arturia, u-he and Korg

Requests 64–65, 19 November 2026. This document compares how three commercial
instruments handle CPU, quality levels, threading and voice management with
what Optisynth does. It treats them as evidence, not as standards.

## Evidence and its limits

| Source | What was read | Limit |
| --- | --- | --- |
| Arturia Matrix-12 V2, SEM V2, Prophet V3, Pigments | Installed XML resource files under `/Library/Arturia/*/resources`, read as data | Parameter names and defaults, not behaviour. No binary was opened |
| u-he Diva | User-guide excerpts found by search ([Diva manual](https://u-he.com/downloads/manuals/plugins/diva/Diva-manual-de-usuario.pdf), [KVR thread](https://www.kvraudio.com/forum/viewtopic.php?p=4904186)) | Older manual edition |
| Korg wavestate Native 1.5.2 | Demangled arm64 symbol names from the already-extracted `wavestate/research/.../native-comparison-2026-11-17/native-arm64-symbols.txt` | Class and method names show design intent, not proven behaviour or algorithms. No code was copied and nothing was run |
| Korg wavestate 3.1.8 hardware | Existing static findings in `wavestate/.../one-knob-package-2026-11-17/performance/findings.txt` | Requested ALSA settings, not measured latency |

The [KVR aliasing thread](https://www.kvraudio.com/forum/viewtopic.php?p=8633877)
shows users finding aliasing in Arturia plugins and asking for oversampling
options. Arturia is not the quality benchmark.

## Findings

**Arturia**
- SEM V2 has internal `PolyBudget` and `PolyMaxAllowedCPU` parameters, which
  suggest polyphony limited by a CPU budget.
- Matrix-12 V2 has per-voice detune and pan for all 12 voices, and a voice
  stealing switch (`EnableSteal`) that defaults to off.
- Prophet V3 has Glide, Legato, Unison and Voices Detune.
- Pigments has per-engine Drift and unison controls: voices, detune, stereo,
  mode and chord.
- No user quality or oversampling setting was found in these instruments.

**u-he Diva**
- Accuracy modes run from draft to divine. Draft has primitive resonance that
  is not zero-delay.
- A separate offline accuracy setting renders at the best quality.
- A multicore switch distributes voices across CPU cores. Forum users report
  mixed results on older CPUs.

**Korg wavestate**

*Voices and cost:*
- `SystemCost`: Reserve, Allocate and Free costs for synthesis and the mixer;
  `SetTotalCpuUsageLimit`; `AssessCurrentSystemCost`.
- `VoicePool`: `StealCost`, `StealForSystemOverrun`, `StaggerStartVoices`,
  `AddToStaggerStartQueue`, `SortVoiceClustersByLevelEstimate`, `Steal`.
- `VoiceCluster`: the voices belonging to one note (unison or layers), with
  level estimates used for steal sorting.
- `TimbreVoiceAllocator`: handlers for legato, mono priority, damper and
  sostenuto, restrike limits per note, exclusive groups and delayed triggers.
- `SynthLimits`: `GetMaxNumVoices`, `GetMaxNumUnisonVoices`, `GetRestrikeLimit`,
  `ShouldPreallocateVoices`.
- Pools appear throughout: `VoicePool`, `VectorEGPool`, `ArpEventPool`,
  `PoolObject`, `DynamicObjectPoolBase`.

*Threading and vectorisation:*
- `AudioThreadManager`: `AssignVoiceVectorToSynthesisThread`,
  `GetSynthesisThreadForNewVoiceCost`, `MoveVoiceVectorToSynthesisThread`,
  `ProcessOnceStart` / `ProcessOnceFinish`, `RegisterMainAudioThread`.
- `SoundingVoiceVector`: note-number, velocity and key-track-pitch *vectors*,
  `ProcessSubRate`, and separate sub-rate and audio-rate parameter blocks.
- `Thread::SetPriorityAndPolicy`.

*DSP:*
- `TPTTwoPoleResonantFilter`, `TPTFourPoleResonantFilter`, `TPTSVF`: the same
  topology-preserving (zero-delay-feedback) approach Optisynth uses.
- `AnalogAdsr` and per-voice pitch-modulation classes, including
  `VoicePortamentoPitchMod`.

*Hardware:* requested ALSA periods of 64 frames × 4 at 48 kHz, `mlockall`, and a
PREEMPT kernel.

## What Optisynth adopts now (0.0.15)

| Lesson | Implementation |
| --- | --- |
| Do not process silent voices (Korg release-complete tracking, every serious engine) | A voice sleeps after 0.5 s below −100 dBFS once released. A woken voice snaps its smoothed controls to their targets, and the pulse wave is zero-mean, so waking adds no glide burst or DC thump |
| Steal by level, not age (Korg level-estimate sort) | Free voices first. Otherwise the quietest held voice is stolen, with ties going to the oldest. New voices are protected until their level is measured |
| Clusters (Korg) and unison (Arturia, Pigments) | Unison 1–8 voices per key with a symmetric detune spread; a key's voices retrigger and release together |
| Optional stealing (Matrix-12 V) | The Steal switch: off means surplus notes are not played |
| Glide (all of them) | Per-voice RC glide from that voice's previous pitch, as on the Prophet-5 |
| Quality levels (Diva) | OB filter quality at 1×, 2× or 4× oversampling. The level agrees within 1 dB in the passband at all three |
| Preallocation (Korg pools) | Already the rule: render allocates nothing, which the tests check |
| Sub-rate controls (Korg) | Already: parameters per 24-frame quantum, audio-rate inputs where needed |
| CPU visibility | DSP load meter in the standalone, showing smoothed render time over real time |

## Deferred, with reasons

- **Multicore voice threading (Korg, Diva).** Measured cost is 7% of one core
  for 8 voices and 16% for 16 unison voices. Threading adds wake-up jitter and
  synchronisation risk for little gain at this size. The design path when
  needed: voice groups as tasks on preallocated worker threads with a per-block
  start/finish barrier, like Korg's `ProcessOnceStart` / `ProcessOnceFinish`.
- **SIMD across voices (Korg voice vectors, VCV `float_4`).** Store module state
  as structure-of-arrays and process four voices per NEON lane. This is a
  per-module rewrite, worth doing once the module set stabilises.
- **CPU budget and overrun stealing (Korg `StealForSystemOverrun`, Arturia
  `PolyMaxAllowedCPU`).** The load meter now provides the measurement. Next:
  per-voice cost estimates and a polyphony cap or steal when load passes a limit.
- **Staggered starts (Korg).** Spread big unison chord starts across blocks.
  Starting is not yet a measured spike.
- **Offline accuracy (Diva).** A render-time quality override for exports.
