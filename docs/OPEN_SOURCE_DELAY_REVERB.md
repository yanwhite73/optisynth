# Open-source delay and reverb candidates

Research shortlist, 17 November 2026; request 35 is logged verbatim in
`USER_REQUESTS.md`. These are candidates, not installed or adopted modules.
Published capabilities/licences below come from author documentation and source.
Porting effort and ordering are our assessment; no CPU or fidelity measurements
have been made for these candidates in Optisynth.

## Candidates

| Source | Musical role and modular value | Published licence / adaptation boundary |
| --- | --- | --- |
| Mutable Instruments Clouds | Original reverb plus looping/granular processing; a natural extension of the Mutable family | Inspected original reverb and looping-player headers carry MIT notices. Extract selected DSP and dependencies, not MCU firmware. |
| Clouds Parasites | Creative modulated/pitch-shifting reverb and extended clocked looping delay | Author publishes software under MIT. Source calls the reverb Oliverb; the current author manual labels it Miverb. Review a pinned revision before choosing names/features. |
| 4ms Dual Looping Delay | Two clock-synchronised looping-delay channels; useful for phrases, repeats and performance | Software MIT; hardware has a separate CC BY-NC-SA licence. Firmware is hardware-oriented C, so more adaptation work than a standalone DSP class. |
| Valley Plateau | Dattorro-derived plate reverb with size, diffusion, filters, hold/clear and tuned operation; extensive modulation controls | Valley publishes GPL-3.0-or-later. Separate the DSP from Rack integration and check the complete dependency/asset notices. |
| CHOW Matrix | A tree of delay nodes with independent feedback, pan and distortion; strong reference for editable delay networks | Main repository BSD-3-Clause; JUCE, chowdsp_utils and GUI dependencies have separate terms to inspect. It is a full plugin rather than a ready Optisynth module. |
| Airwindows Galactic | Wide ambient reverb, useful for pads and evolving spaces | Repository MIT. Extract a reviewed DSP implementation and provide our own contextual controls. Other Airwindows effects can follow individually. |
| DaisySP ReverbSc and delay utilities | Stereo reverb and smaller building blocks for custom effects | Main DaisySP is MIT, but ReverbSc is explicitly LGPL-2.1 in DaisySP-LGPL. Do not describe the whole combined collection as MIT. |

## Primary evidence checked

Mutable: [original Clouds reverb](https://github.com/pichenettes/eurorack/blob/master/clouds/dsp/fx/reverb.h)
and [looping sample player](https://github.com/pichenettes/eurorack/blob/master/clouds/dsp/looping_sample_player.h).
The reverb has fixed delay storage and LFO constants referencing 32 kHz. Our
current Mutable adapters run at 48 kHz, so copying it directly into that rate
would alter time/pitch behaviour. Decide and test rate conversion or deliberate
retuning. Read the complete selected dependency closure before adoption.

Parasites: [author overview/licence](https://mqtthiqs.github.io/parasites/),
[Clouds manual](https://mqtthiqs.github.io/parasites/clouds.html) and
[Oliverb source](https://github.com/mqtthiqs/parasites/blob/master/clouds/dsp/fx/oliverb.h).
The manual describes mono-in/stereo-out creative reverb, modulated parameters and
tempo-synchronised looping/ping-pong delay. Its naming differs from the source;
record this instead of assuming every downloaded fork has the same modes.
This is not original Make Noise or Lexicon code, nor evidence of an exact emulation.

4ms: [DLD repository/README](https://github.com/4ms/DLD),
[software/hardware licence](https://github.com/4ms/DLD/blob/master/LICENSE), and
[manufacturer page](https://4mscompany.com/dld.php). The repository describes an
STM32 implementation with long per-channel buffers. A desktop port needs bounded
buffer duration, replacement of board/codec/clock I/O and review of the firmware
manual/changelog and relevant processing sources. Those full reviews are pending.

Valley: [Plateau manual](https://valleyaudio.github.io/rack/plateau/),
[repository](https://github.com/ValleyAudio/ValleyRackFree),
[main licence](https://github.com/ValleyAudio/ValleyRackFree/blob/main/LICENSE.md)
and [additional notices](https://github.com/ValleyAudio/ValleyRackFree/blob/main/LICENSE-dist.md).
The author exposes CV control across the panel and describes a modulated reverb
tank. This makes it a strong module/UI reference as well as a DSP candidate.

CHOW: [repository/README](https://github.com/Chowdhury-DSP/ChowMatrix),
[licence](https://github.com/Chowdhury-DSP/ChowMatrix/blob/main/LICENSE) and
[dependency manifest](https://github.com/Chowdhury-DSP/ChowMatrix/blob/main/.gitmodules).
Our adaptation would bound node count and prepare topology edits off-thread,
regardless of the author's expandable-tree description. Full manual/source and
dependency review remain pending.

Airwindows: [Galactic author's notes](https://www.airwindows.com/galactic/) and
[source/licence repository](https://github.com/airwindows/airwindows).
The author explicitly notes that moving Bigness can produce crashing sounds;
classify controls by safe modulation behaviour rather than routing arbitrary
fast LFOs into every parameter. No universal modulation-rate promise is justified.

Daisy: [DaisySP](https://github.com/daisyaudio/DaisySP),
[DaisySP-LGPL](https://github.com/daisyaudio/DaisySP-LGPL) and
[ReverbSc header](https://github.com/daisyaudio/DaisySP-LGPL/blob/main/Source/Effects/reverbsc.h).
The inspected API exposes stereo processing, feedback and damping; its fixed
buffer and Init result need checking at every supported rate. The LGPL collection
depends on DaisySP. Dependency and distribution review is still required.

## Proposed Optisynth integration

Start with a simple clocked delay and one Mutable reverb after stereo integration;
our inherited first-party effects are also available as a baseline. Then review
Parasites for a more playable effect and 4ms for a dedicated looping-delay model.
Plateau is a strong alternative if its licence fits the distribution plan.
CHOW Matrix is particularly useful for a later delay-network editor. This order
does not replace the requested complete Spectral engine and sequencer work.

Each effect should be independently instantiable with contextual UI, stereo audio
ports, insert and wet-only send/return use, and stable parameter identities for
the matrix. Offer useful modulation of time, feedback, filtering, diffusion and
freeze where the algorithm supports it. Use the sequencers' shared clock contract
for tempo sync; specify whether time changes glide in pitch or crossfade.

Internal delay/reverb feedback can stay encapsulated inside a module. External
patching of filters or pitch shifters into a feedback path requires an explicit
causal feedback design: the current graph rejects cycles. Do not merely disable
that check. Expose headroom, bounded gain and a clear/reset action, preserving
deliberate musical self-oscillation without undefined numeric output.

Prepare only reachable effects, allocate their buffers off-thread and prefer
shared global send effects over a reverb per voice. Silence at the input must not
unload a tail or frozen buffer. Define tail behaviour on bypass, rerouting and
removal; existing graph replacement does not promise long-tail continuity.
Ordinary parameter state and captured loop/freeze audio are different persistence
requirements. Decide explicitly when captured sound becomes a saved asset.

## Review boundary and acceptance before use

This pass only reads documentation/source excerpts. No candidate was cloned,
built, loaded, installed or connected to audio/MIDI/network. Intended adapters
process caller-owned buffers and parameters; they should neither open devices
or sockets nor write external files. Those are design requirements, not audited
claims about every upstream plugin/firmware path.

Before adoption, pin revision and hashes; read all available relevant README,
manual, changelog and dependency/source contents; document state writes, ports,
dependencies, build/removal procedure and what could go wrong if assumptions are
false. Missing review could import board I/O, unbounded allocation or unsuitable
parameter behaviour. Removal should drop the registered adapter and dependency
closure; patch loading must report unavailable module types without corrupting state.

Qualify impulse/tail renders, stereo behaviour, rates, time modulation, freeze,
tempo changes, maximum feedback, nonfinite input containment, save/recall and
multiple-instance CPU/memory. Confirm no callback allocation and no truncation of
tails during supported graph edits. None of these checks is claimed complete yet.
