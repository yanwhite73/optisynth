# Optisynth

Yan's modular instrument project: spectral, sample, granular, virtual-analogue and
physical-modelling components sharing envelopes, modulation, routing and effects.
The patch structure will be directly inspectable/editable by agentic tools.

Local project: `/Users/yanwhite/Documents/Music docs/Code/optisynth`.
Repository: https://github.com/yanwhite73/optisynth.git .

## Project memory

- [User instructions, verbatim](docs/USER_REQUESTS.md)
- [Current status and next steps](docs/STATUS.md)
- [Architecture and performance contract](docs/ARCHITECTURE.md)
- [Research, manuals, schematics and source references](docs/RESEARCH.md)
- [Other open-source module makers](docs/OPEN_SOURCE_MODULE_MAKERS.md)
- [Contextual rack interface](docs/CONTEXTUAL_UI.md)
- [Sample player and asset loading](docs/SAMPLE_PLAYER.md)
- [Source snapshot provenance](docs/source_snapshot_20261117.json)

## Current implementation

The modular audio runtime uses original Plaits variable-shape and sine oscillators,
modal percussion, externally excited resonator and bass-drum DSP, plus Mutable's
digital SVF filters. Spectral AHDSFR/ADSR envelopes and LP/HP/BP/notch filters remain
interchangeable options. There are 18 module types, including a WAV sample player,
mixer and VCA.
Only output-reachable modules are constructed. Audio/control buffers and numeric
bindings are prepared off-thread. One envelope can drive amplitude and cutoff;
the core also accepts multiple sources and an explicit mixer.

The native standalone app loads contextual panels from the same parameter schema
as DSP. A Matriarch-inspired rack presents coloured sections, module-specific
control arrangements and clickable routing sockets, with an enlarged focus view.
It supports Caps Lock typing notes, a piano keyboard, patch save/open,
typed input routing and device-rate conversion around the 48 kHz DSP core.
Add independent module instances, connect their audio/control ports, select the
output and save the complete rack. Older instrument patches import automatically.
This first voice is monophonic; polyphony, granular engines, hardware MIDI
and an agent API remain upcoming. Samples can excite a resonator or form its
parallel attack layer. Their decoded assets are shared; playheads are independent.
The inherited Spectral engine remains a separate compatibility reference.

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

Build the native Apple Silicon app with the existing JUCE 8.0.8 installation:

```sh
agent-workload run --owner YOUR_OWNER --kind build --purpose 'Build Optisynth app' --wait 60 -- sh tools/build_app.sh
```

The app is `app/Builds/MacOSX/build/Release/Optisynth.app`. Click **Start audio**,
then use the piano or turn on **Caps Lock** and play A/W/S/E/D/F…; Z/X changes
octave. Try the six starter patches; edit their panels directly or select a panel
and click **Focus module** for larger controls and a connection list. Click a socket
to choose a compatible connection; hover to see existing routes. Add modules with
the menu above the rack, or use a panel's selector to replace its type. Use a
Mixer to combine audio and a VCA with an envelope to articulate continuous sources.
Required inputs must be connected before a module can become part of the audible
output. Unrouted modules remain editable without consuming DSP. Patches use
`.optisynth` JSON files. No audio input or hardware MIDI port is opened.

Choose **Sample into resonator** or **Sample attack + resonator** for built-in
examples. **Load WAV…** or drop a WAV onto the sample panel, then adjust pitch,
root note, start/end and level. **Loop while held** repeats until note release.
WAVs are loaded in the background, only for connected modules. Mono/stereo PCM or
float is supported up to 32 MiB per file; stereo is mixed to mono. Saved patches
refer to the original files, so keep those files in place. Missing/changed files
leave the playing patch intact. See the sample document for remaining limits.

`plugin/Optisynth.jucer` has a separate product/bundle/plugin identity and disables
automatic installation. It still wraps the compatibility engine; no new VST3 has
been built or installed. Inherited qualification scripts need porting to the new
identity before use. Historical Optispectral instructions are in
`docs/optispectral_README.md`, not the current Optisynth build contract.

Reference PDF/ZIP files are local under `docs/references/` and git-ignored. Source
links and download hashes remain in docs. Adopted DSP provenance and integration
review are in `third_party/mutable/PROVENANCE.json` and
[Mutable first slice](docs/MUTABLE_FIRST_SLICE.md).
