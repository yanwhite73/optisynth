#!/bin/sh
# Reference comparison runs (request 66). Run through agent-workload; offline only.
set -eu
cd "$(dirname "$0")/../.."
B=build/reference; R=$B/render_au; O=$B/optisynth_render; S=tools/reference_render/settings
A="python3 tools/reference_render/analyse.py"
echo "## aliasing (bare sawtooth, filter open)"
for n in 60 84 96; do
  $R render OBS2 $B/sem-saw$n.wav 1.5 "$n:100:0:1.4" @$S/sem_bare.txt > /dev/null; $A alias $B/sem-saw$n.wav $n
  $R render PRO3 $B/pro-saw$n.wav 1.5 "$n:100:0:1.4" @$S/prophet_bare.txt "Voices Detune=0" > /dev/null; $A alias $B/pro-saw$n.wav $n
  for q in q0 "" q2; do $O saw$n$q $B/opti-saw$n$q.wav; $A alias $B/opti-saw$n$q.wav $n; done
done
echo "## cpu (8-note chord, 10 s, effects off)"
CH="48:100:0:9.5,52:100:0:9.5,55:100:0:9.5,59:100:0:9.5,60:100:0:9.5,64:100:0:9.5,67:100:0:9.5,71:100:0:9.5"
$R render OBS2 $B/sem-chord.wav 10 "$CH" @$S/sem_bare.txt
$R render PRO3 $B/pro-chord.wav 10 "$CH" @$S/prophet_bare.txt
$O chord8 $B/opti-chord.wav
echo "## spread (eight sequential A4 notes; Prophet at its default Voices Detune)"
SP="69:100:0:0.4,69:100:0.5:0.9,69:100:1:1.4,69:100:1.5:1.9,69:100:2:2.4,69:100:2.5:2.9,69:100:3:3.4,69:100:3.5:3.9"
$R render OBS2 $B/sem-spread.wav 4.2 "$SP" @$S/sem_bare.txt > /dev/null; $A spread $B/sem-spread.wav 8 0.5 0.4
$R render PRO3 $B/pro-spread.wav 4.2 "$SP" @$S/prophet_bare.txt > /dev/null; $A spread $B/pro-spread.wav 8 0.5 0.4
$O spread $B/opti-spread.wav; $A spread $B/opti-spread.wav 8 0.5 0.4
echo "## latency"
$O latency x
