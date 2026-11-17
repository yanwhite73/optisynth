#!/bin/sh
# Run through agent-workload --kind build. Local build and offline processor checks only.
# Does not install the AU, run auval, open the standalone app, or interact with any DAW.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
xcodebuild -project plugin/Builds/PartialsMacOSX/Optispectral.xcodeproj \
  -target 'Optispectral - All' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > build/plugin_release_build_2026-11-09.log 2>&1
echo 'AU and Standalone release build passed'
xcodebuild -project plugin/Tests/Builds/PartialsStressMacOSX/PartialsStress.xcodeproj \
  -target 'PartialsStress - ConsoleApp' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > build/stress_release_build_2026-11-09.log 2>&1
echo 'Release stress build passed'
plugin/Tests/Builds/PartialsStressMacOSX/build/Release/PartialsStress presets-imported 20 ABC \
  > build/stress_release_run_2026-11-09.log 2>&1
echo 'Release stress test passed'
codesign --verify --deep --strict plugin/Builds/PartialsMacOSX/build/Release/Optispectral.component
codesign --verify --deep --strict plugin/Builds/PartialsMacOSX/build/Release/Optispectral.app
file plugin/Builds/PartialsMacOSX/build/Release/Optispectral.component/Contents/MacOS/Optispectral
echo 'Local bundle signatures verified; nothing installed'
