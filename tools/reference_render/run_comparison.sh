#!/bin/sh
# Reference comparison runs (requests 66-67). Run through agent-workload; offline only.
# Every Arturia render starts from an init preset converted from Arturia's own preset
# file (arturia_preset.py), then applies the few listed test overrides.
set -eu
cd "$(dirname "$0")/../.."
B=build/reference; R=$B/render_au; O=$B/optisynth_render; S=tools/reference_render/settings
A="python3 tools/reference_render/analyse.py"; P=/Library/Arturia/Presets
python3 -I tools/reference_render/arturia_preset.py "$P/SEM V2/Factory/Factory/Init Voice 1 (Sawtooth Wave)" $B/sem-params.json $S/sem_init_saw.txt
python3 -I tools/reference_render/arturia_preset.py "$P/Prophet V3/Factory/Factory/Default" $B/prophet-params.json $S/prophet_default.txt "/Library/Arturia/Prophet V3/resources"
# Test overrides: low-pass fully open, no resonance, effects off; Prophet uses only its Pro5 path.
SEM_TEST='VCF Mode=0'
PRO_TEST='Pad Mixer to Pro5 Filter=0'
FX_OFF_SEM='Chorus On/Off=0'
echo "## aliasing (init sawtooth, filter open)"
for n in 60 84 96; do
  $R render OBS2 $B/sem-saw$n.wav 1.5 "$n:100:0:1.4" @$S/sem_init_saw.txt "$SEM_TEST" "$FX_OFF_SEM" "Delay On/Off=0" > /dev/null; $A alias $B/sem-saw$n.wav $n
  $R render PRO3 $B/pro-saw$n.wav 1.5 "$n:100:0:1.4" @$S/prophet_default.txt "$PRO_TEST" "Chorus On-Off=0" "Delay On-Off=0" > /dev/null; $A alias $B/pro-saw$n.wav $n
  for q in q0 "" q2; do $O saw$n$q $B/opti-saw$n$q.wav; $A alias $B/opti-saw$n$q.wav $n; done
done
echo "## cpu (8-note chord, 10 s, init presets, effects off)"
CH="48:100:0:9.5,52:100:0:9.5,55:100:0:9.5,59:100:0:9.5,60:100:0:9.5,64:100:0:9.5,67:100:0:9.5,71:100:0:9.5"
$R render OBS2 $B/sem-chord.wav 10 "$CH" @$S/sem_init_saw.txt "$FX_OFF_SEM" "Delay On/Off=0"
$R render PRO3 $B/pro-chord.wav 10 "$CH" @$S/prophet_default.txt "Chorus On-Off=0" "Delay On-Off=0"
$O chord8 $B/opti-chord.wav
echo "## spread (eight sequential A4 notes, init presets unchanged)"
SP="69:100:0:0.4,69:100:0.5:0.9,69:100:1:1.4,69:100:1.5:1.9,69:100:2:2.4,69:100:2.5:2.9,69:100:3:3.4,69:100:3.5:3.9"
$R render OBS2 $B/sem-spread.wav 4.2 "$SP" @$S/sem_init_saw.txt > /dev/null; $A spread $B/sem-spread.wav 8 0.5 0.4
$R render PRO3 $B/pro-spread.wav 4.2 "$SP" @$S/prophet_default.txt > /dev/null; $A spread $B/pro-spread.wav 8 0.5 0.4
$O spread $B/opti-spread.wav; $A spread $B/opti-spread.wav 8 0.5 0.4
echo "## latency"
$O latency x
