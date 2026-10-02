# Implementation and validation evidence

## Status

The implementation is accepted after the original-code baseline, final Debug/Release validation, independent code review and a test-only fixture correction. The final evidence below distinguishes initial failed runs from the successful affected rerun. All 13 tasks are complete; archival remains a separate step.

## Production boundary

- `SceneNode.h` adds owned `FSceneComponentChange` identities, occurrence flags and a trailing `FSceneChange::ComponentChanges` vector.
- Private `SceneComponentChanges` enumerates live registered instances, pairs `(TypeId, InstanceId)`, invokes descriptor equality, and merges sorted unique occurrence sets. Empty optional slots and opaque envelopes stay outside typed facts.
- `FMutation::PrepareChanges` compares authoritative storage with the final scratch node. Absent slots and different generations have no before node. Deleted-node identities come from authoritative storage. All callback calls and new allocations/merges finish before the existing first authoritative write in `Commit`.
- Model/Camera/Light compatibility effects use the existing typed value semantics only for affected types. The current mutation masks receive those effects before query dirtiness is collected; accumulated historical masks are merged only into outgoing changes. `StageNode` retains local/parent/enabled/no-op validation and drops its duplicated three typed classifications.
- `BeginSynchronization` requests live-instance Added facts explicitly, preserves outstanding occurrences, publishes settings without component facts, and retains revision and single-consumer behavior.
- Renderer, SceneEditing, component registry/container, hierarchy implementation, operation IDs, reflected schemas and persistence formats are unchanged. Existing user-facing editing operations continue through the same domain service.

## Test coverage

`Source/Tests/Scene/SceneComponentChangeTests.cpp` uses literal identities and flags to cover repeated types, ordering, type IDs used as instance IDs, rename and replacement, live/empty/opaque states, occurrence unions, acknowledgement, retained old values, slot reuse, Clear/subtrees, initial synchronization, hierarchy-derived versus local state, restored children, built-in semantic masks and convenience setters.

`SceneComponentFailureTests.cpp` forces descriptor Equal in the new preparation boundary by changing Name before whole-node equality. A two-node case prepares the first node before the second throws; Reparent and KeepChildren exercise paths that bypass StageNode. Get failure watches the authoritative `std::any` address, so candidate validation cannot satisfy its sentinel. Failure checks retain complete scene nodes, handles/generations, order/roots/children, World/enabled, counts/settings/revision, full pending changes including facts, and a warm triangle ray hit/index state. The first postfailure ray is measured before any additional warming.

`SceneComponentAllocationTests.cpp` uses the existing Debug-only test executable allocator/PDB adapter, targeting `Hyperion::BuildSceneComponentDifference` and recoverable map emplace frames containing `FSceneComponentPair`. Cases cover AddNode, RemoveSubtree, BeginSynchronization, Clear and KeepChildren, require `WasInjected`, compare the same full snapshot, and retry initial synchronization. The final Debug execution demonstrated all five exact PDB-matched injections and unchanged state; no production fault switch was added.

`SceneComponentDomainTests.cpp` exercises typed component values, structural component operations, Duplicate, KeepChildren Undo/Redo with new generations, and Edit/Undo/Redo callback failure against committed history, cursor, State/NextState/SavedState, selection and dirty state. The plugin-private entry owns the existing real `FSceneTestTarget` and passes only `ISceneEditTarget&`, `FScene&` and `FTaskSystem&` into the test TU under `Source/Tests`. This keeps shared test snapshots reusable without cross-module private includes. No fake diff or transport success/error is used as the transactional oracle.

The existing SceneEditing baseline is preserved: rejected Create can consume NextState and finish interaction, while Undo/Redo finish interaction before attempting restoration. New history fault fixtures start without an active interaction and verify the already committed history boundary; they do not claim a broader rollback contract or alter SceneEditing production code.

## Existing baseline accepted before production

Root acceptance is recorded in `out/maintainability/Orchestration/M07BaselineAcceptance.json`, with six original test-source snapshots under `out/maintainability/M07/BaselineSource`. Actual baseline build HEAD was `0f0dbc35912160631c4c197179154b1e2881e6f1`.

Both configurations built `scene_tests`, `scene_query_tests`, `scene_render_tests` and `automation_scene_tests`. Debug CPU tests passed on their first run. The new native fixture initially expected a local packet rebuild, but source inspection showed unchanged view/content takes full packet reuse; only that new assertion was corrected to exact Collection/Preparation/Packet reuse with zero prepared input builds/local packet reuse. The corrected Debug `scene_rendering` passed in 21.84 seconds and Release all four CTests passed in 5.38 seconds. All preexisting assertions stayed intact. The root retained both the failed fixture run and the correction/retry evidence.

The production-only intermediate freeze is `M07ProductionCompileFiles.json` (SHA256 `0156EB96724FC3E74EC166C091728662FCDEA13B775F47F9F086CFCD8FF108A8`). Root verified every hash, built Debug `scene_tests`, and passed the old `scene_management` baseline in 1.45 seconds; logs are `M07ProductionCompileBuild.log` and `M07ProductionCompileTests.log` in the orchestration directory. This is an intermediate gate, not acceptance of the newly added facts tests.

## Final acceptance

Both configurations built `scene_tests`, `automation_scene_tests`, `scene_query_tests`, `scene_render_tests` and `scene_boundary_tests`; Debug additionally built `scene_dispatch_failure_tests`. The initial final configure failed because the external domain test TU was registered before module source grouping. Only the Automation test CMake ordering was repaired; `M07FinalRetryBuild.log` records both successful configurations.

The first final Debug run passed five of six checks and Release passed four of five. Their sole failure was the newly added opaque replacement fixture: the existing registered-component `Remove` API cannot remove an opaque envelope. Independent review M07-C01 and root source/runtime verification agreed. The repair only saves the known-component collection and reconstructs the candidate before adding the replacement envelope; all assertions and production code remain unchanged. Both configurations rebuilt the affected target and `scene_management` then passed (Debug 1.48 seconds, Release 0.58 seconds). The preceding automation, query, boundary, render and Debug allocation results remain valid. Original failed logs are preserved rather than counted as passing.

The accepted tests cover literal registered identity/occurrence facts, ownership and acknowledgement, generation reuse, synchronization, built-in compatibility masks and setters, callback/Get failure before publication, shared-domain history, warm-query selectivity and native full packet reuse. Debug allocation cases 0–4 each require actual `WasInjected` and compare full authority, pending facts and the first postfailure warm ray; their outputs are retained in `M07FinalDebugTestDetail.log`. The affected `scene_management` rerun also reaches the built-in/setter and callback tests that were after the original fixture failure.

Scoped formatting, semantic naming of 17 changed translation units and the repaired single TU, owned filenames/include casing, dependency boundaries, whitespace and strict OpenSpec checks passed. All six original baseline assertions remain preserved: five source files are byte-identical; SceneOperationTests only adds the new domain test entry declaration and call.

The independent review covered all 31 source/artifact paths and their consumers, confirmed one test-only issue and no production defect, and rechecked the frozen hashes. The targeted final review closes M07-C01 after reading the exact repair and actual successful reruns. Root separately read the production, new tests, documentation and transaction/query/history paths. Reports, hashes and raw logs are under `out/maintainability/Orchestration/`; only the task/evidence text is updated after source review and is separately reviewed before staging. No archive or push is performed.
