#!/bin/sh
# Run through agent-workload; native snapshots open a temporary app window,
# never an audio device. DSP changes still require make test asan-modular.
set -eu
cd "$(dirname "$0")/.."
sh tools/build_app.sh
app/Builds/MacOSX/build/Release/Optisynth.app/Contents/MacOS/Optisynth --snapshot "$PWD/build/sequencer-ui-review"
cat build/sequencer-ui-review/ui-check.txt build/sequencer-ui-review/sequencer-check.txt build/sequencer-ui-review/pad-check.txt
