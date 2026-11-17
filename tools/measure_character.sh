#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
python3 reverse-engineer/tools/probe_character.py inspect > partials/build/character_metadata.txt
for stage in baseline feedback taps; do
  python3 reverse-engineer/tools/probe_character.py "$stage" run > "partials/build/character_${stage}_before.txt"
  cp "reverse-engineer/renders/probe_character/${stage}_analysis.json" "partials/build/character_${stage}_before.json"
  echo "$stage reference comparisons complete"
done
