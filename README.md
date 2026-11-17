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
- [Source snapshot provenance](docs/source_snapshot_20261117.json)

## Current implementation

The modular audio runtime uses original Plaits variable-shape and sine oscillators,
modal percussion, externally excited resonator and bass-drum DSP, plus Mutable's
digital SVF filters. Spectral AHDSFR/ADSR envelopes and LP/HP/BP/notch filters remain
interchangeable options. There are 17 module types, including mixer and VCA.
Only output-reachable modules are constructed. Audio/control buffers and numeric
bindings are prepared off-thread. One envelope can drive amplitude and cutoff;
the core also accepts multiple sources and an explicit mixer.

The native standalone app loads contextual panels from the same parameter schema
as DSP. It supports Caps Lock typing notes, a piano keyboard, patch save/open,
typed input routing and device-rate conversion around the 48 kHz DSP core.
Add independent module instances, connect their audio/control ports, select the
output and save the complete rack. Older instrument patches import automatically.
This first voice is monophonic; polyphony, samples/granular engines, hardware MIDI
and an agent API remain upcoming. Excitation currently comes from another module.
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
octave. Try the four starter patches; select a module on the left to reveal its
controls and input connections. Add modules with the menu above the rack. Use a
Mixer to combine audio and a VCA with an envelope to articulate continuous sources.
Required inputs must be connected before a module can become part of the audible
output. Unrouted modules remain editable without consuming DSP. Patches use
`.optisynth` JSON files. No audio input or hardware MIDI port is opened.

`plugin/Optisynth.jucer` has a separate product/bundle/plugin identity and disables
automatic installation. It still wraps the compatibility engine; no new VST3 has
been built or installed. Inherited qualification scripts need porting to the new
identity before use. Historical Optispectral instructions are in
`docs/optispectral_README.md`, not the current Optisynth build contract.

Reference PDF/ZIP files are local under `docs/references/` and git-ignored. Source
links and download hashes remain in docs. Adopted DSP provenance and integration
review are in `third_party/mutable/PROVENANCE.json` and
[Mutable first slice](docs/MUTABLE_FIRST_SLICE.md).
