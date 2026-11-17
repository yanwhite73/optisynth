#!/bin/sh
# Run through agent-workload --kind test. Uses existing original WAVs only; never loads a
# third-party plugin. Rebuild build/partials-render before invoking this comparison suite.
set -eu
cd "$(dirname "$0")/.."
export OPENBLAS_NUM_THREADS=1 VECLIB_MAXIMUM_THREADS=1
PROBE_DIR=probe_fx2 python3 ../reverse-engineer/tools/probe_fx_analyse.py all > build/effect_comparison.txt
echo 'effect comparison written'
env DUR=4.5 NOTELEN=4.2 sh ../reverse-engineer/tools/probe_render.sh \
  ../reverse-engineer/renders/probe_unison --partials-only > build/unison_render.log
python3 ../reverse-engineer/tools/probe_unison_analyse.py \
  ../reverse-engineer/renders/probe_unison > build/unison_comparison.txt
echo 'unison comparison written'
sh renders/ab_compare.sh > build/preset_comparison.txt
echo 'preset comparison written'
