# Optisynth

Documentation is sorted by topic; `docs/README.md` is the directory index. Keep
project memory in `docs/project`, contracts in `docs/architecture`, interface work
in `docs/ui` and source studies in the appropriate `docs/research` subfolder.
Historical receipts and inherited Optispectral files retain their original paths
inside the evidence; `docs/project/document_locations_20261117.json` maps locations.

Read `docs/project/USER_REQUESTS.md`, `docs/project/STATUS.md`, `docs/architecture/ARCHITECTURE.md` and
`docs/research/RESEARCH.md` before continuing. Append every new user request/correction
verbatim to USER_REQUESTS when it arrives, then reconcile STATUS and the plan.

This is the separate modular project. `../partials` is Optispectral; do not use it
as the edit target. `docs/verification/source_snapshot_20261117.json` records the exact copied
working source. The starting repository had no commits. On 17 November Yan
explicitly requested regular commits and pushes (USER_REQUESTS.md request 21).
Bundle implementation, checks and documentation into coherent delivery commits
(request 49); do not make separate feedback-only commits. Push checked milestones
to origin; this supersedes the
inherited no-commit restriction for Optisynth. Preserve independent local backups.

## Build delivery discipline

The ONE user-facing standalone app is
`app/Builds/MacOSX/build/Release/Optisynth.app`. Always link that exact app in
handoffs. Do not hand out temporary build folders, alternate app copies or a new
location when a build tool fails. `tools/build_app.sh` compiles in
`build/app-candidate`, then uses `tools/deliver_app.py` to verify/copy the bundle,
archive the previous app under `build/app-archive`, and write the adjacent
`Optisynth-build.json` with version, hash and actual qualification status.
Fallback compilation must use the same delivery helper. A failed build/copy must
preserve the previous delivery. Never label a preview as UI-qualified, and never
launch, quit or replace a running user's process as part of delivery.
Read the failure/prevention record in `docs/project/ENGINEERING_LOG.md`, request
53. Capture corrections immediately; a log entry does not fulfil the correction.

## Sequencer product discipline

Request 54 corrects the gap between extensive research and basic implementations.
Use the saved source/manual/UI studies to define concrete musical operations and
then exercise them in the actual editor. A feature count, compilation or DSP
checks cannot establish a mature interface. Keep edit, playing and queued states
separate, demonstrate complete multi-part patches, and inspect normal/minimum-size
native captures before delivery. Record omissions explicitly; do not imply that
one pattern module implements the researched specialist sequencer families.

Use the workspace workload launcher and inherited host rules. Keep offline
qualification distinct from VST3 packaging/installation and user-performed Live
verification. Do not drive Live. Retain the independent implementation boundary
recorded in the inherited design doc. Research sources, evidence, uncertainties
and implementation consequences belong in docs, with direct source links.

Only prepare modules and resources in the active graph. Construction, loading,
graph sorting and allocation must happen off the audio thread. Do not equate
zero instantaneous output with an unused module: modulators, feedback and tails
can still be active. Benchmark actual paths before calling them real-time ready.

Inherited plugin files and comparison utilities describe Optispectral. They are
historical starting material, not proof that the new modular engine is integrated
or qualified as a plugin. Establish distinct plugin identity before any new build.
