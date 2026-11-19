# Optisynth

Yan's modular instrument project: spectral, sample, granular, virtual-analogue and
physical-modelling components sharing envelopes, modulation, routing and effects.
The patch structure will be directly inspectable/editable by agentic tools.

Local project: `/Users/yanwhite/Documents/Music docs/Code/optisynth`.
Repository: https://github.com/yanwhite73/optisynth.git .

## Project memory

- [Documentation folders and index](docs/README.md)
- [User instructions, verbatim](docs/project/USER_REQUESTS.md)
- [Current status and next steps](docs/project/STATUS.md)
- [Architecture and performance contract](docs/architecture/ARCHITECTURE.md)
- [Research, manuals, schematics and source references](docs/research/RESEARCH.md)
- [Other open-source module makers](docs/research/modules/OPEN_SOURCE_MODULE_MAKERS.md)
- [Contextual rack interface](docs/ui/CONTEXTUAL_UI.md)
- [Sample player and asset loading](docs/architecture/SAMPLE_PLAYER.md)
- [Complete Spectral engine and modulation matrix](docs/architecture/SPECTRAL_ENGINE_INTEGRATION.md)
- [Open-source sequencer research and model taxonomy](docs/research/sequencers/SEQUENCER_MODELS_RESEARCH.md)
- [Sequencer modes, shared UI and specialist modules](docs/ui/SEQUENCER_UI_ARCHITECTURE.md)
- [Earlier sequencer/Optiseq findings and isolated timing prototype](docs/research/sequencers/SEQUENCERS_AND_ARPEGGIATORS.md)
- [Open-source delay/reverb candidates](docs/research/modules/OPEN_SOURCE_DELAY_REVERB.md)
- [Source snapshot provenance](docs/verification/source_snapshot_20261117.json)

## Four-track Performance sequencer — 0.0.9

The Performance module has four independent tracks, eight 64-step patterns per
track, quantized pattern launch and a sixteen-row phrase chain. Each track has
separate note, gate, pitch-bend and CV outputs. Pattern lengths/rates, ties,
slides, ratchets, conditions, probability, mute/solo/fill, protected variations
and scoped undo/redo support editing and performing a multi-part patch.
[Controls, research mapping and limits](docs/architecture/PERFORMANCE_SEQUENCER.md).

The app opens **Performance / Four currents** and its editor, with audio off.
Press **PLAY**. Click a bank to edit it; its triangle queues playback. **CHAIN**
opens the arrangement. **Performance / Clockwork garden** is the second starter.
The earlier Pattern / Motion, Euclidean and Stage modules remain available.

Always open [Optisynth](app/Builds/MacOSX/build/Release/Optisynth.app).
That stable delivery location contains 0.0.9. All 251 modular checks pass normally
and under ASan/UBSan; full-bank/stereo recall, native component interactions and
four visual captures pass. Desktop window launch and listening remain unverified
in the current execution context. The adjacent `Optisynth-build.json` records the
version, binary hash, checks and previous-app archive. See [STATUS](docs/project/STATUS.md).

## Current implementation

The modular audio runtime uses original Plaits variable-shape and sine oscillators,
modal percussion, externally excited resonator and bass-drum DSP, plus Mutable's
digital SVF filters. Spectral AHDSFR/ADSR envelopes and LP/HP/BP/notch filters remain
interchangeable options. There are 27 module types, including a WAV sample player,
mixer, VCA, complete Spectral engine, independent LFO and stereo utilities.
Performance, Pattern, Euclidean and Stage sequencers share an audio clock and route notes to
independent destinations. [Sequencer contract](docs/architecture/FIRST_SEQUENCERS.md).
Only output-reachable modules are constructed. Audio/control buffers and numeric
bindings are prepared off-thread. One envelope can drive amplitude and cutoff;
the core also accepts multiple sources and an explicit mixer.

The native standalone app loads contextual panels from the same parameter schema
as DSP. A Matriarch-inspired rack presents coloured sections, module-specific
control arrangements and clickable routing sockets, with an enlarged focus view.
It supports Caps Lock typing notes, a piano keyboard, patch save/open,
typed input routing and device-rate conversion around the 48 kHz DSP core.
Add independent module instances, connect their audio/control/note ports, select the
output and save the complete rack. Older instrument patches import automatically.
This first voice is monophonic; polyphony, granular engines, hardware MIDI
and an agent API remain upcoming. Samples can excite a resonator or form its
parallel attack layer. Their decoded assets are shared; playheads are independent.
The complete Spectral engine now runs as one stereo module with its internal
matrix, arp and effects intact. Its own voice engine remains internal; the rack
keyboard still supplies a shared monophonic note context. Load imported `.ptch`
presets and adjust paged live controls. The matrix connects module control outputs
to parameter destinations and saves their signed depths separately from base values.

## Offline development

Use the shared workload launcher with your session's owner ID:

```sh
agent-workload run --owner YOUR_OWNER --kind test --purpose 'Qualify Optisynth foundation' --wait 60 -- make test-graph asan-graph test
```

The C++17 modular core includes a reviewed, pinned MIT DSP subset under
`third_party/mutable`; it needs no external installation. `build/partials-render`
retains the legacy name and `.ptch` format for regression comparisons; it renders
the compatibility engine. `make test-modular asan-modular` checks the new runtime
and creates short source/starter audio examples under `build/`.
`make test-timing asan-timing` also checks the clock/event foundation independently.
The modular tests exercise sequencer timing, note routes and note-release ownership.

Build the native Apple Silicon app with the existing JUCE 8.0.8 installation:

```sh
agent-workload run --owner YOUR_OWNER --kind build --purpose 'Build Optisynth app' --wait 60 -- sh tools/build_app.sh
```

The app is `app/Builds/MacOSX/build/Release/Optisynth.app`. Click **Start audio**,
then use the piano or turn on **Caps Lock** and play A/W/S/E/D/F…; Z/X changes
octave. Try the fifteen starter patches; edit their panels directly or select a panel
and click **Focus module** for larger controls and a connection list. Click a socket
to choose a compatible connection; hover to see existing routes. Add modules with
the menu above the rack, or use a panel's selector to replace its type. Use a
Mixer to combine audio and a VCA with an envelope to articulate continuous sources.
Required inputs must be connected before a module can become part of the audible
output. Unrouted modules remain editable without consuming DSP. Patches use
`.optisynth` JSON files. No audio input or hardware MIDI port is opened.

Builds compile in `build/app-candidate`. `tools/deliver_app.py` verifies and copies
the result to the same app location above, preserving the previous app under
`build/app-archive`. Fallback builds use this same delivery step. Temporary build
folders are internal; handoffs always link the stable app. Delivery does not
imply native UI qualification; the receipt states what was actually checked.

For sequencing, choose **Pattern / melodic voice**, **Euclidean / modal percussion**
or **Stage / pulsing voice**, then **Start audio** and **Play seq**. All three share
the BPM control. **Stop seq** releases notes; **Restart** returns to beat zero.
Select a Pattern cell to change pitch/velocity or turn its note off (double-click
also toggles a rest). Euclidean exposes hits/length/rotation around a generated
ring. Stage widths follow pulse duration; select Repeat, Hold or Rest per stage.
The pale outline shows selection; the gold marker shows the audio playhead.
The `notes` output can drive several inputs, for example both an oscillator and
its envelope. Unpatched `notes` inputs use the keyboard. Destination voices are
currently monophonic; independent held-note arp, chord modes and a general rack
conductor remain planned. Performance supplies its own four-track chain. Rack
version 5 saves tempo and all pattern banks; playback state is transient.

Choose **Sample into resonator** or **Sample attack + resonator** for built-in
examples. **Load WAV…** or drop a WAV onto the sample panel, then adjust pitch,
root note, start/end and level. **Loop while held** repeats until note release.
WAVs are loaded in the background, only for connected modules. Mono/stereo PCM or
float is supported up to 32 MiB per file; stereo is mixed to mono. Saved patches
refer to the original files, so keep those files in place. Missing/changed files
leave the playing patch intact. See the sample document for remaining limits.

Choose **Spectral stereo + external LFO**, then **Matrix**, to change the cutoff
assignment or add another route. **Spectral audio + LFO into resonator** shows
audio and actual internal modulation feeding another module. The Spectral panel
offers a section selector and a next-page button; **Load Spectral patch...** reads
the existing imported `.ptch` format. Whole presets are embedded when saving a rack.
Structural waveform/arp/internal-routing editing is not yet ported from the older
Spectral editor. Use **Stereo to mono** explicitly before mono processors.

`plugin/Optisynth.jucer` has a separate product/bundle/plugin identity and disables
automatic installation. It still wraps the compatibility engine; no new VST3 has
been built or installed. Inherited qualification scripts need porting to the new
identity before use. Historical Optispectral instructions are in
`docs/legacy/optispectral/optispectral_README.md`, not the current Optisynth build contract.

Reference PDF/ZIP files are local under `docs/references/` and git-ignored. Source
links and download hashes remain in docs. Adopted DSP provenance and integration
review are in `third_party/mutable/PROVENANCE.json` and
[Mutable first slice](docs/research/modules/MUTABLE_FIRST_SLICE.md).
