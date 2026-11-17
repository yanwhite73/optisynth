#!/bin/sh
# Local plugin qualification only: no installation, app launch, or host interaction.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/character_presets
for name in 'ib Filth Bass' 'fn HiHat' 'hr Tortured Lead 1' 'hr Tortured Lead 2'; do
  cp "presets-imported/$name.ptch" "build/character_presets/$name.ptch"
done
xcodebuild -project plugin/Tests/Builds/PartialsStressMacOSX/PartialsStress.xcodeproj \
  -target 'PartialsStress - ConsoleApp' -configuration Debug -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=YES ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=YES \
  ENABLE_THREAD_SANITIZER=NO build > build/character_stress_asan_build.log 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Debug/PartialsStress presets-imported 20 ABC \
  > build/character_stress_asan_run.log 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Debug/PartialsStress build/character_presets 12 AB \
  > build/character_named_asan_run.log 2>&1
echo 'ASan/UBSan processor stress passed, including the named presets'
xcodebuild -project plugin/Builds/PartialsMacOSX/Optispectral.xcodeproj \
  -target 'Optispectral - All' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > build/character_plugin_release_build.log 2>&1
echo 'AU and Standalone release build passed'
xcodebuild -project plugin/Tests/Builds/PartialsStressMacOSX/PartialsStress.xcodeproj \
  -target 'PartialsStress - ConsoleApp' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > build/character_stress_release_build.log 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Release/PartialsStress presets-imported 20 ABC \
  > build/character_stress_release_run.log 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Release/PartialsStress build/character_presets 12 AB \
  > build/character_named_release_run.log 2>&1
echo 'Release processor stress passed, including the named presets'
codesign --verify --deep --strict plugin/Builds/PartialsMacOSX/build/Release/Optispectral.component
codesign --verify --deep --strict plugin/Builds/PartialsMacOSX/build/Release/Optispectral.app
shasum -a 256 plugin/Builds/PartialsMacOSX/build/Release/Optispectral.app/Contents/MacOS/Optispectral \
  plugin/Builds/PartialsMacOSX/build/Release/Optispectral.component/Contents/MacOS/Optispectral \
  > build/character_release_sha256.txt
echo 'Bundle signatures verified and hashes recorded'
