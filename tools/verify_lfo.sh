#!/bin/sh
# Rerender only our implementation; all original audio stays untouched.
set -eu
cd "$(dirname "$0")/../.."
for stage in lfo_origin lfo_cycle lfo_start filth_split; do
  python3 reverse-engineer/tools/probe_manual.py "$stage" partials > "partials/build/lfo_${stage}_verified.txt"
done
python3 reverse-engineer/tools/probe_character.py baseline partials > partials/build/lfo_named_verified.txt
cp reverse-engineer/renders/probe_character/baseline_analysis.json partials/build/lfo_named_verified.json
echo 'LFO, Filth isolation and all named comparisons completed'
