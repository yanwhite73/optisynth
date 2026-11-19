#!/bin/sh
# Reference comparison runs (requests 66-70). Run through agent-workload; offline only.
# Each Arturia instrument loads an init preset WHOLE (preset= puts Arturia's own preset
# file into the AU state) with no parameter overrides, so its routing, voice count and
# play mode are the preset's own. Without this, the AUs load their last factory patch.
set -eu
cd "$(dirname "$0")/../.."
B=build/reference; R=$B/render_au; O=$B/optisynth_render; A="python3 tools/reference_render/analyse.py"
P=/Library/Arturia/Presets
SEM="preset=$P/SEM V2/Factory/Factory/Init Voice 1 (Sawtooth Wave)"
PRO="preset=$P/Prophet V3/Factory/Factory/Default"
echo "## aliasing (init sawtooth patches as saved)"
for n in 60 84 96; do
  $R render OBS2 $B/sem-saw$n.wav 1.5 "$n:100:0:1.4" "$SEM" > /dev/null; $A alias $B/sem-saw$n.wav $n
  $R render PRO3 $B/pro-saw$n.wav 1.5 "$n:100:0:1.4" "$PRO" > /dev/null; $A alias $B/pro-saw$n.wav $n
  for q in q0 "" q2; do $O saw$n$q $B/opti-saw$n$q.wav; $A alias $B/opti-saw$n$q.wav $n; done
done
echo "## cpu (10 s chords; SEM init has 8 voices, Prophet Default 5)"
CH8="48:100:0:9.5,52:100:0:9.5,55:100:0:9.5,59:100:0:9.5,60:100:0:9.5,64:100:0:9.5,67:100:0:9.5,71:100:0:9.5"
CH5="59:100:0:9.5,60:100:0:9.5,64:100:0:9.5,67:100:0:9.5,71:100:0:9.5"
$R render OBS2 $B/sem-chord.wav 10 "$CH8" "$SEM"
$R render PRO3 $B/pro-chord.wav 10 "$CH5" "$PRO"
$O chord8 $B/opti-chord8.wav
$O chord5 $B/opti-chord5.wav
echo "## spread (eight sequential A4 notes, init patches as saved)"
SP="69:100:0:0.4,69:100:0.5:0.9,69:100:1:1.4,69:100:1.5:1.9,69:100:2:2.4,69:100:2.5:2.9,69:100:3:3.4,69:100:3.5:3.9"
$R render OBS2 $B/sem-spread.wav 4.2 "$SP" "$SEM" > /dev/null; $A spread $B/sem-spread.wav 8 0.5 0.4
$R render PRO3 $B/pro-spread.wav 4.2 "$SP" "$PRO" > /dev/null; $A spread $B/pro-spread.wav 8 0.5 0.4
$O spread $B/opti-spread.wav; $A spread $B/opti-spread.wav 8 0.5 0.4
echo "## latency"
$O latency x
