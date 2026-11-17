#!/bin/sh
# Reuses every original WAV. Run via agent-workload --kind test after building the CLI.
set -eu
cd "$(dirname "$0")/../.."
for stage in filters crusher filter_check; do
  if [ ! -f "reverse-engineer/renders/probe_fx3/${stage}_before_effects.json" ]; then
    cp "reverse-engineer/renders/probe_fx3/${stage}_analysis.json" \
       "reverse-engineer/renders/probe_fx3/${stage}_before_effects.json"
  fi
  python3 reverse-engineer/tools/probe_fx3.py "$stage" partials \
    > "partials/build/fx3_${stage}_verification.txt"
  echo "Compared $stage against saved originals"
done
sh partials/renders/ab_compare.sh > partials/build/preset_comparison_fx3_final.txt
echo 'Preset comparison complete'
