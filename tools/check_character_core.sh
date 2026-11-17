#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
python3 reverse-engineer/tools/probe_character.py taps2 run > partials/build/character_taps2_before.txt
python3 reverse-engineer/tools/analyse_feedback_shape.py > partials/build/character_feedback_models.txt
echo 'Additional tap probes and scalar feedback models measured'
make -C partials all test asan > partials/build/character_core_checks.log 2>&1
for stage in baseline feedback taps2; do
  python3 reverse-engineer/tools/probe_character.py "$stage" partials > "partials/build/character_${stage}_first_fix.txt"
done
echo 'Core tests and first fix comparisons complete'
