# Mala and Beads in use

Assessment of Optisynth 0.0.36 and 0.0.37, 27 November 2026. Both versions have the same Mala processor wrapper, DSP files and faceplate code.

## Fidelity by function

| Area | Evidence | What a player can expect |
|---|---|---|
| Grain engine, delay, reverb and media | All 36 files under `third_party/mutable/beads` have the same Git blob IDs as their upstream counterparts at `f4cfa7697095a28d292bff758beb51deb49b5a86`. This includes resources, trigger generation and the no-input synth. | The grains, envelopes, pitch shift, delay taps, feedback processing, wavetable banks and digital tape colour come from the original algorithms. |
| Clock, pitch and grain address | The wrapper feeds the firmware’s Seed events and pitch/time CV parameters. The three saved examples below exercise those sockets, including the change from clocked delay to free delay when Seed is disconnected. | The manual’s patch recipes translate directly when their inputs are explicitly cabled. In the rack, a pitch CV value of 0.2 corresponds to one octave. |
| Quality and input stage | Mala runs the processor at the chosen media rate inside a 48 kHz rack. It adds a 12-sample bridge and implements the hardware main loop, CV conditioning and input gain in software. The release tests exercise dry gain, bandwidth ordering, mono/stereo input and gated/frozen audio. | Sample-rate reduction, saturation and wow/flutter are part of the port. Absolute hardware noise floor, analogue gain, converter response and end-to-end latency have no hardware A/B here. |
| Freeze | The firmware checks its effective Freeze lamp, combining gate and button, before accepting a media change. Mala checks its button parameter and discards the computed gate-inclusive block status. Its lamp also reads only the button. | A patched gate holds recording, but the lamp can stay dark. In a controlled test, changing quality while holding the gate muted Mala for 250 ms. Holding the panel button gave zero silent blocks. Use the button when holding audio through a quality change. |
| Routing and state | Beads provides independent L/R sockets, hardware left-only summing and an optional grain trigger on R. Mala exposes one stereo rack output. Its R-gate setting places the pulses in the audio pair. A fresh graph enters wavetable mode immediately when both inputs are empty. Saved racks retain controls and connections but omit the recorded buffer. | No automatic left-only jack normalization or separately typed trigger output. A held audio collage must be refilled after recall. Hardware waits ten seconds for no-input mode and can restore a backed-up frozen buffer. |
| Panel | Large controls, four attenurandomizers and quality selection follow the original layout. The software also exposes latched seed, gain, gate-on-R and custom assign amounts as named controls. Firmware custom assign depths use 8-bit values divided by 256. Mala accepts continuous values up to 1. | Most front-panel instructions are recognizable. The software has different alternate gestures, display behavior and CV granularity. |

For the integrity check, compute SHA-1 over the Git blob header and each file’s bytes and compare against the recursive Git tree from the pinned revision. This checks against the upstream repository independently of the local provenance manifest. The original hardware main loop, controls and power-cycle storage sit outside this 36-file subset. [Pinned upstream tree](https://github.com/pichenettes/eurorack/tree/f4cfa7697095a28d292bff758beb51deb49b5a86/beads) · [Hardware main loop](https://github.com/pichenettes/eurorack/blob/f4cfa7697095a28d292bff758beb51deb49b5a86/beads/beads.cc) · [UI state-change guard](https://github.com/pichenettes/eurorack/blob/f4cfa7697095a28d292bff758beb51deb49b5a86/beads/ui.h)

The Freeze comparison used a 440 Hz mono input, Bright digital, fully dry output, fixed unity input gain, and twelve-sample blocks. Freeze went high at 1 s and the switch to Cold digital was requested at 1.25 s. The threshold for a silent block was absolute amplitude below 0.000001 on all twelve samples. The dry signal makes a codec restart visible even if the frozen buffer itself is quiet. This was an offline wrapper test. No physical Beads recording or subjective listening comparison was available.

## Three playable manual studies

Open the preset browser and search **Mala**. All three files are under `patches/mala` in source control. The installed collection is `~/Music/Optisynth/Presets/Mala Studies`. Each saves its sound source, modulation cables, panel and sequence, and uses no external samples.

| Patch | What to try | Basis in the manual |
|---|---|---|
| **Mala - Copper Relay** | Play the sparse 104 BPM copper pattern. Move **Delay multiple** while leaving **Size** fully clockwise. The dry strike marks the pulse against the repeats. **Hold slice** catches the current division. | Infinite grain size selects delay. A quarter-note clock enters Seed, Density selects a division and Time selects a multiple. Uses Sunny tape. [Beads as a delay](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#beads-as-a-delay) |
| **Mala - Shards in Reverse** | Start the pattern, let a phrase pass, then engage **Hold phrase**. In the sequencer change a CV 1 step to select a different piece of that recording. CV 2 fires the reads. **Reverse length** changes how much of each gesture comes back. | The manual suggests sequencing Time and triggering Seed to rearrange recorded slices. Here a struck metal string supplies new material, the grains run backwards, and a little dry attack remains as a rhythmic reference. [Patch ideas](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#patch-ideas) |
| **Mala - Lantern Choir** | Start the pitch sequence. Lower **Grain blend** to hear the continuous wavetable melody, then raise it to hear notes overlap. **Wave bank** changes the underlying instrument. The source can sustain when transport stops. | Both audio inputs are empty. Pitch CV tracks the notes. Feedback selects one of eight banks and Dry/wet sets the oscillator/grain balance. [Granular wavetable synth](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/#beads-as-a-granular-wavetable-synth) |

## Audio checks

Each saved file was loaded through the app’s patch parser and sample preparation and rendered for 16 seconds of transport plus three seconds of tail, at 48 kHz stereo and saved output gain. For each preset an independent process repeated the render and another disconnected the specified cable. The repeat null was exactly zero for all three. The last column measures the RMS difference relative to the original over seconds 1–16. A positive difference is possible when two signals differ in phase or pitch. It measures the cable’s effect, not sound quality.

| Patch | Peak dBFS | Playing RMS dBFS | Cable removed | Difference dB |
|---|---:|---:|---|---:|
| Copper Relay | −4.3 | −23.7 | CV 2 → Seed | −1.4 |
| Shards in Reverse | −3.4 | −23.3 | CV 1 → Time | −9.4 |
| Lantern Choir | −5.0 | −20.6 | CV 1 → Pitch | +2.1 |

The local audition WAVs are in `build/mala-manual`. The sounds and their defining connections are in the committed preset files.

To repeat the audio checks with 0.0.37 or later, run `python3 tools/check_mala_studies.py` through the shared test launcher. Passing `--library "$HOME/Music/Optisynth/Presets/Mala Studies"` tests the installed copies. The release executable’s own offline renderer reproduced all three peak, RMS and cable-difference results above to the displayed precision.

## Copper Undertow

Yan identified movement in the delay as the part of Copper he might use. This variation starts with his 96 BPM edit and keeps the pitch, rests, velocities and three movement assignments to the struck voice. Seed still receives the quarter-note clock on CV 2. The Time LFO sweeps the repeat spacing in about 17.6 seconds. CV 1 has an independent 64-step lane lasting four bars. Its rises increase both the wet level and feedback, with the longest rise in bar four. The two amounts are exposed as **Return lift** and **Feedback lift**. The player's saved Copper edit remains available separately.
