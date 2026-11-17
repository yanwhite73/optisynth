#!/bin/sh
# Local AU/VST3/Standalone qualification. Does not install or open an audio device.
set -eu
cd "$(dirname "$0")/.."
report_dir=${1:-build/manual_qualification}
mkdir -p "$report_dir/presets"
make all test asan > "$report_dir/core_checks.log" 2>&1
echo 'Core and importer checks passed for final source'
for name in 'ib Filth Bass' 'fn HiHat' 'hr Tortured Lead 1' 'hr Tortured Lead 2'; do
  cp "presets-imported/$name.ptch" "$report_dir/presets/$name.ptch"
done
xcodebuild -project plugin/Tests/Builds/PartialsStressMacOSX/PartialsStress.xcodeproj \
  -target 'PartialsStress - ConsoleApp' -configuration Debug -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=YES ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=YES \
  ENABLE_THREAD_SANITIZER=NO build > "$report_dir/stress_asan_build.log" 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Debug/PartialsStress presets-imported 20 ABCDE \
  > "$report_dir/stress_asan_run.log" 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Debug/PartialsStress "$report_dir/presets" 12 AB \
  > "$report_dir/named_asan_run.log" 2>&1
echo 'ASan/UBSan processor stress passed, including the named presets'
xcodebuild -project plugin/Builds/PartialsMacOSX/Optispectral.xcodeproj \
  -target 'Optispectral - All' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > "$report_dir/plugin_release_build.log" 2>&1
echo 'AU, VST3 and Standalone release build passed'
xcodebuild -project plugin/Tests/Builds/PartialsStressMacOSX/PartialsStress.xcodeproj \
  -target 'PartialsStress - ConsoleApp' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > "$report_dir/stress_release_build.log" 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Release/PartialsStress presets-imported 20 ABCDEV \
  > "$report_dir/stress_release_run.log" 2>&1
plugin/Tests/Builds/PartialsStressMacOSX/build/Release/PartialsStress "$report_dir/presets" 12 AB \
  > "$report_dir/named_release_run.log" 2>&1
echo 'Release processor stress passed, including the named presets'
codesign --verify --deep --strict plugin/Builds/PartialsMacOSX/build/Release/Optispectral.component
codesign --verify --deep --strict plugin/Builds/PartialsMacOSX/build/Release/Optispectral.app
codesign --verify --deep --strict plugin/Builds/PartialsMacOSX/build/Release/Optispectral.vst3
shasum -a 256 plugin/Builds/PartialsMacOSX/build/Release/Optispectral.app/Contents/MacOS/Optispectral \
  plugin/Builds/PartialsMacOSX/build/Release/Optispectral.component/Contents/MacOS/Optispectral \
  plugin/Builds/PartialsMacOSX/build/Release/Optispectral.vst3/Contents/MacOS/Optispectral \
  > "$report_dir/release_sha256.txt"
echo 'Bundle signatures verified and hashes recorded'
