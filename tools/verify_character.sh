#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
make -C partials all test asan > partials/build/character_core_checks.log 2>&1
echo 'Core and importer checks passed'
python3 reverse-engineer/tools/analyse_feedback_shape.py > partials/build/character_feedback_channels.txt
for stage in baseline feedback taps2 tap3; do
  python3 reverse-engineer/tools/probe_character.py "$stage" partials > "partials/build/character_${stage}_verified.txt"
  cp "reverse-engineer/renders/probe_character/${stage}_analysis.json" "partials/build/character_${stage}_verified.json"
done
python3 -I partials/tools/migrate_env_depth.py > partials/build/character_import_migration.txt
echo 'Offline comparisons and local imported-patch migration complete'
