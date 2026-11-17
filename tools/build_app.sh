#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
python3 tools/make_app_project.py
/Users/yanwhite/JUCE/Projucer.app/Contents/MacOS/Projucer --resave app/OptisynthApp.jucer > build/app-project.log 2>&1
if ! xcodebuild -project app/Builds/MacOSX/Optisynth.xcodeproj -configuration Release -jobs 2 ARCHS=arm64 CODE_SIGN_IDENTITY=- build > build/app-build.log 2>&1; then
    rg -n -A 5 'error:|BUILD FAILED|failed:' build/app-build.log
    exit 1
fi
tail -5 build/app-build.log
