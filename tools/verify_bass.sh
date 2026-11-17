#!/bin/sh
# Build our implementation, then compare preserved references. No new original renders.
set -eu
cd "$(dirname "$0")/../.."
make -C partials all > partials/build/bass_build.log 2>&1
echo 'Bass corrections built'
for stage in low_pm spread_gain spread_check pm_symmetry pm_symmetry_check filth_split; do
  python3 reverse-engineer/tools/probe_manual.py "$stage" partials > "partials/build/bass_${stage}_verified.txt"
done
python3 reverse-engineer/tools/analyse_spread_gain.py spread_check > partials/build/bass_spread_fit_verified.txt
python3 reverse-engineer/tools/probe_character.py baseline partials > partials/build/bass_named_verified.txt
cp reverse-engineer/renders/probe_character/baseline_analysis.json partials/build/bass_named_verified.json
echo 'Controlled and named-preset comparisons completed'
