#!/bin/sh
# Offline qualification of the waveform/filter/AVG pass. No installation or app launch.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
xcodebuild -project plugin/Tests/Builds/PartialsStressMacOSX/PartialsStress.xcodeproj \
  -target 'PartialsStress - ConsoleApp' -configuration Debug -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=YES ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=YES \
  ENABLE_THREAD_SANITIZER=NO build > build/fx3_stress_asan_build.log 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Debug/PartialsStress presets-imported 20 ABC \
  > build/fx3_stress_asan_run.log 2>&1
echo 'ASan/UBSan processor stress passed'
xcodebuild -project plugin/Builds/PartialsMacOSX/Optispectral.xcodeproj \
  -target 'Optispectral - All' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > build/fx3_plugin_release_build.log 2>&1
echo 'AU and Standalone release build passed'
xcodebuild -project plugin/Tests/Builds/PartialsStressMacOSX/PartialsStress.xcodeproj \
  -target 'PartialsStress - ConsoleApp' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > build/fx3_stress_release_build.log 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Release/PartialsStress presets-imported 20 ABC \
  > build/fx3_stress_release_run.log 2>&1
echo 'Release processor stress passed'
codesign --verify --deep --strict plugin/Builds/PartialsMacOSX/build/Release/Optispectral.component
codesign --verify --deep --strict plugin/Builds/PartialsMacOSX/build/Release/Optispectral.app
shasum -a 256 plugin/Builds/PartialsMacOSX/build/Release/Optispectral.app/Contents/MacOS/Optispectral \
  plugin/Builds/PartialsMacOSX/build/Release/Optispectral.component/Contents/MacOS/Optispectral \
  > build/fx3_release_sha256.txt
echo 'Signatures verified and build hashes recorded'
