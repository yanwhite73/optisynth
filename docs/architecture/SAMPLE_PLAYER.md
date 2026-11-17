# Sample player — implementation contract

Request 30, 17 November 2026. Next playable milestone after the VCV architecture
research. Extend the current mono runtime with immutable sample assets and a
separate playback state per node. Preserve existing DSP and patch migration.

## Mechanism and acceptance

`sample.player` outputs mono audio. Root note, transpose, start/end trim, level and
loop mode are scalar parameters. A note trigger restarts the trimmed region;
one-shots finish naturally, loops stop on note release. This first slice retains
the existing shared note context; event cables/polyphony/voice groups remain queued.
Source rates are converted during pitched playback. Interpolation is an initial
quality trade-off, with later antialiasing refinement explicitly separate.

The graph continues to contain only node/port/parameter descriptions. The app's
patch document adds per-node sample references; Runtime receives a bank of shared
immutable decoded assets. Prepare only assets required by the reachable graph.
No file I/O, allocation or ownership destruction on the render path. One bounded
loader handles requests; newer edits supersede stale loading results. Failed loads
preserve the playing patch. Bound individual and aggregate sample memory.

First file support: little-endian WAV, mono/stereo PCM and float, with stereo
averaged to the current mono graph. Store the original path plus a content
fingerprint; audio bytes remain external. Clearly disclose that presets depend on
those files; collection/portable bundles and stereo processing are later work.
Missing/changed active files fail with a useful message. Dormant references do not
force loading. Two generated, original sample/resonator starters need no external
sample library. Existing sample-free v1 patches continue to import.

UI: sample-specific panel, Load WAV button/file drop, filename, waveform and trim
controls. Keyboard/piano and existing typed socket routes remain. Verify normal
and minimum sizes, failure messages, file reload/recall and independent instances.

Tests: bounds/format parsing including truncated/odd chunks; sample rates/pitch,
trim, looping/release, retrigger and independent playback; no render allocation;
missing/changed asset transaction; dormant pruning; full patch/audio recall;
old starters; ASan/UBSan. Qualify native app and save captures before handoff.

## Evidence inspected before implementation

- [Microsoft RIFF specification](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-):
  chunk sizes exclude headers/padding, WORD alignment, chunks may vary in order.
- [WAVEFORMATEX](https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatex)
  and [WAVEFORMATEXTENSIBLE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-waveformatextensible):
  block alignment, channel/rate/container fields and valid-bit alignment.
- Existing reviewed JUCE 8.0.8 `juce_FileDragAndDropTarget.h` read: drag-interest
  callbacks must be cheap; actual file loading starts only after a drop. This uses
  the app's existing JUCE build; no new third-party decoder/library is adopted.

The first-party decoder reads chosen files only; it opens no audio/MIDI/network
ports and writes no source files. The app saves patch metadata only when requested.
Malformed inputs must be rejected before excessive allocation or rendering. This
contract does not authorize installations, Live actions or hardware input capture.

## Implementation details and limits (0.0.5)

- WAV: PCM 8/16/24/32-bit, IEEE float 32/64-bit, including extensible headers;
  mono/stereo, 8–384 kHz. Stereo is averaged, so antiphase material can cancel.
  32 MiB per input file and per decoded mono asset; 128 MiB of unique active
  assets per prepared rack. Decode can temporarily add one asset and its raw file
  before rejecting an oversized bank. Old/new graphs coexist during crossfade,
  so 128 MiB is not a process-wide memory limit.
- Playback uses linear interpolation at the fixed 48 kHz engine rate, short edge
  fades, and an independent playhead per instance. Root note is MIDI 48 by default;
  pitch spans ±36 semitones. Trim is normalized and takes effect on the next note.
  Crossed/equal trims select a bounded one-frame region. Loop release is at most
  5 ms. Seamless crossfade loops, stereo and pitch-dependent antialiasing are later.
- `optisynth.rack` v2 adds node-to-sample references. v1 racks and v1 instrument
  patches still read. Chosen file paths are absolute; files remain external.
  Fingerprints use FNV-1a-64 for accidental-change detection, not authentication.
  Loading a saved patch checks its referenced content; explicit Load WAV accepts
  a changed file. Cached immutable audio stays valid if its source later disappears.
- One background worker owns one replaceable request and one result. A generation
  token rejects stale work; decoding checks cancellation between bounded reads and
  loops. UI polling installs a prepared plan. Scalar/structural edits cancel a
  pending load so an old request cannot overwrite newer edits. Failed active loads
  preserve the current patch. Dormant references retain metadata without file I/O.
- The generated original click in both sample/resonator starters is built in,
  reproducible and independent of external samples. The same click is synthesized
  during preparation, never during rendering.
- File-dialog and dropped-file callbacks use the same loading path. The test
  harness covers the drop handler/interest checks, not a Finder-driven drag gesture.
  The app opens with audio off; native verification never opens an audio device.
