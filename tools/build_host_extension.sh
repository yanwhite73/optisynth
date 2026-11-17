#!/bin/sh
# Managed foreground build, no installation or host interaction.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host_extension
xcodebuild -quiet -project plugin/Builds/PartialsMacOSX/Optispectral.xcodeproj \
  -target 'Optispectral - All' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > build/host_extension/plugin_build.log 2>&1
echo 'Optispectral release bundles built'
xcodebuild -quiet -project plugin/Tests/Builds/PartialsStressMacOSX/PartialsStress.xcodeproj \
  -target 'PartialsStress - ConsoleApp' -configuration Release -jobs 2 ARCHS=arm64 ONLY_ACTIVE_ARCH=YES \
  CODE_SIGN_IDENTITY=- ENABLE_ADDRESS_SANITIZER=NO ENABLE_UNDEFINED_BEHAVIOR_SANITIZER=NO \
  ENABLE_THREAD_SANITIZER=NO build > build/host_extension/stress_build.log 2>&1
echo 'Offline host qualification helper built'
