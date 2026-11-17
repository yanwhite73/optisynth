# Optisynth local source backups

Run `tools/backup_source.py --output ../backups/optisynth` through the shared
workload launcher. It makes a dated archive, external SHA-256 sidecar and per-file
manifest, then verifies the complete archive. It now snapshots Optisynth rather
than the parent Optispectral checkout.

Included: source, tests, tools, documentation, AGENTS, app headers/project, our own
`.optisynth` patches, plugin definitions and copied preset reader/schemas.
Excluded: build output, SDKs, original binaries, third-party presets/audio, Git
metadata and downloaded PDF/ZIP reference files. The reference
source catalogue and checksum receipts are included; the manual archive remains
under docs/references for separate copying when desired.

Restore with `tools/backup_source.py --verify ARCHIVE --extract-to NEW_DIRECTORY`.
The destination must not exist. Restore verifies every file before writing. The
restored optisynth directory builds the core/planner with Apple clang and Python;
JUCE and private fixture data are separate. No network, commit or upload is involved.

This source snapshot protects against working-file mistakes on this Mac. Retain
an independent drive copy for machine-loss recovery.

On 17 November 2026 Yan also authorized regular commits and pushes to
https://github.com/yanwhite73/optisynth.git (request 21). Push coherent tested
milestones; keep this archive workflow as an independent backup. Record the exact
commit and confirm remote branch equality rather than treating a local commit as
a successful push. Generated builds and downloaded references stay ignored.
