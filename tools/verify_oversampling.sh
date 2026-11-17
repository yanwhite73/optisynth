#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
make -C partials all test asan > partials/build/oversampling_core_checks.log 2>&1
echo 'Core and importer checks passed'
for stage in matrix spread pm_validation; do
  python3 reverse-engineer/tools/probe_manual.py "$stage" partials > "partials/build/oversampling_${stage}_verified.txt"
done
python3 reverse-engineer/tools/probe_character.py baseline partials > partials/build/oversampling_named_verified.txt
cp reverse-engineer/renders/probe_character/baseline_analysis.json partials/build/oversampling_named_verified.json
echo 'Controlled and named-preset reference comparisons complete'
