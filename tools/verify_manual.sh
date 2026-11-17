#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
make -C partials all test asan > partials/build/manual_core_checks.log 2>&1
echo 'Core and importer checks passed'
for stage in matrix spread; do
  python3 reverse-engineer/tools/probe_manual.py "$stage" partials > "partials/build/manual_${stage}_verified.txt"
done
python3 reverse-engineer/tools/probe_character.py baseline partials > partials/build/manual_named_verified.txt
cp reverse-engineer/renders/probe_character/baseline_analysis.json partials/build/manual_named_verified.json
python3 -I partials/tools/migrate_matrix_zero.py > partials/build/manual_import_migration.txt
echo 'Preserved-reference comparisons and imported-patch migration complete'
