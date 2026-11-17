#!/bin/sh
# Run via agent-workload --kind test. Existing reference-host approval applies.
# Originals are never overwritten; probe_fx3.py aborts if a reference already exists.
set -eu
cd "$(dirname "$0")/../.."
for stage in preflight normalise holdout; do
  cp "reverse-engineer/renders/probe_fx3/${stage}_analysis.json" \
     "reverse-engineer/renders/probe_fx3/${stage}_before_normalisation.json"
  python3 reverse-engineer/tools/probe_fx3.py "$stage" partials
done
python3 reverse-engineer/tools/probe_fx3.py filters
python3 reverse-engineer/tools/probe_fx3.py crusher
sh partials/renders/ab_compare.sh > partials/build/preset_comparison_fx3.txt
echo 'Preset comparison written to partials/build/preset_comparison_fx3.txt'
