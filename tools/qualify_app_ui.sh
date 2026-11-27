#!/bin/sh
# Run through agent-workload; native snapshots open a temporary app window,
# never an audio device. DSP changes still require make test asan-modular.
set -eu
cd "$(dirname "$0")/.."
sh tools/build_app.sh
app/Builds/MacOSX/build/Release/Optisynth.app/Contents/MacOS/Optisynth --snapshot "$PWD/build/reviews/sequencer"
cat build/reviews/sequencer/ui-check.txt build/reviews/sequencer/sequencer-check.txt build/reviews/sequencer/pad-check.txt build/reviews/sequencer/panel-browser-check.txt
