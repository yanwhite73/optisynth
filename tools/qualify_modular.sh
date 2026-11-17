#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
sh tools/build_app.sh
if ! make test asan-modular > build/modular-qualification.log 2>&1; then
    tail -60 build/modular-qualification.log
    exit 1
fi
rg 'passed|OK|Ran |FAIL' build/modular-qualification.log
app/Builds/MacOSX/build/Release/Optisynth.app/Contents/MacOS/Optisynth --snapshot "$PWD/build/ui-review"
cat build/ui-review/ui-check.txt
