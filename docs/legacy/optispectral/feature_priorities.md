# Remaining synthesis work — 10 November 2026

Read-only census: `tools/audit_feature_usage.py`, detailed results in
`feature_usage.json`. No original plugin was run. Collections differ: 2,636
imported patch filenames and 2,645 original files in the backup sound banks.
Earlier 2,648-file corpus totals include other fixtures and duplicate names.

| Missing behavior | Presets containing evidence | Collection |
| --- | ---: | --- |
| Unsupported matrix destination | 1,928 | Imported |
| Unsupported matrix source | 143 | Imported |
| Filter RM, enabled filter and nonzero base depth | 218 | Imported |
| Envelope pitch scaling nonzero | 1,217 | Original backup |
| Precision below 100 | 1,101 | Original backup |
| Envelope velocity scaling nonzero | 891 | Original backup |
| Glide ON/HELD/BEND | 564 | Original backup |

These are presence counts, not measured audible failures. Unused envelopes are
included. Zero-depth matrix rows are retained because another row can change
their depth. The RM count misses zero-base amounts activated by modulation.

The most common unsupported destination names are arp gate (296), main envelope
release (293), main envelope decay (251), oscillator 1 spread detune (242),
oscillator 2 spread detune (199), main envelope attack (179), filter 1 route
balance (145), and slot 1 wet (135). Unsupported source spellings include Mod
Wheel Sub (79), Pitch Wheel (55), Aftertouch Sub (10) and Glide (4).

Next order:

1. Resolve unsupported source spellings against the manual and existing measured
   behavior; build small fixtures before aliasing semantically different sources.
2. Implement matrix envelope/arp/spread destinations and envelope scaling in
   bounded groups. For effect wet modulation, establish how per-voice sources
   combine before applying them to the shared effect chain. The new host wet
   controls do not establish the original internal matrix law.
3. Measure and implement glide/voice modes and Precision with independent code.
4. Implement Filter RM from isolated behavioral measurements, then return to
   full-preset timbre comparisons and CPU work.

Existing measured formulas, approximate effects, high-rate/polyphonic CPU limits
and resonator harmonic differences still need work. The census changes priority;
it does not make the current build a complete Spectral match.
