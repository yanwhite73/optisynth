# Optisynth

Read `docs/USER_REQUESTS.md`, `docs/STATUS.md`, `docs/ARCHITECTURE.md` and
`docs/RESEARCH.md` before continuing. Append every new user request/correction
verbatim to USER_REQUESTS when it arrives, then reconcile STATUS and the plan.

This is the separate modular project. `../partials` is Optispectral; do not use it
as the edit target. `docs/source_snapshot_20261117.json` records the exact copied
working source. The starting repository had no commits. On 17 November Yan
explicitly requested regular commits and pushes (USER_REQUESTS.md request 21).
Commit and push coherent, checked milestones to origin; this supersedes the
inherited no-commit restriction for Optisynth. Preserve independent local backups.

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
