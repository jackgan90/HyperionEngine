# Development quality audit — 2026-09-12

## Scope and conclusion

The user authorized an independent audit of the current scene-owned camera/light implementation and repair of confirmed in-scope problems. Baseline is `ee585cc88fd60a22a29dc2103cd4dafe6d234221` on `main`; the starting working tree contained 122 changed paths (80 tracked modifications and 42 new files), with nothing staged. The reviewer was created with no inherited conversation context and did not participate in implementation.

The initial review covered hierarchy transactions, node/legacy/source/native validation, resource loading and persistence, geometry plus metadata publication, frame ownership/material semantics, both render pipelines and CSM, and Viewer integration. The main agent independently traced and reproduced the findings before editing. All three confirmed P2 findings were repaired and closed by the same independent reviewer. No remaining in-scope finding was identified in the focused re-review.

The user explicitly deferred five follow-ups: cross-version screenshot threshold failures, performance trigger cells, the earlier intermittent shadow-retirement failure, dispatch allocation-failure injection, and complete native GUI mouse replay. This audit does not resolve or claim acceptance of those items. Ordinary regression tests for affected render paths still ran; no investigation of the excluded intermittent failure was performed.

## Findings and disposition

| ID | Confirmed behavior | Repair and regression evidence | Disposition |
| --- | --- | --- | --- |
| IR-01 / P2 | Copy construction or assignment retained a private resolved token while public material inputs could be changed, allowing non-scene light values. | The resolved frame has a private weak self-owner. Validation requires that live original allocation, and semantic resolution also validates. No strong ownership cycle is introduced. `CheckResolvedFrameOwnership` covers both copy forms across Global/Frame/Scene, validation/semantic/build rejection, successful original rendering and allowed unbound copies. | Closed by independent re-review. |
| IR-02 / P2 | A name/world/material edit through legacy Find/Update could persist inherited hiding as local model visibility. | Unchanged effective visibility preserves the authored bit; unchanged World preserves Local without inversion. `SetModelVisible` is the explicit authored setter while disabled, documented in the public interface. `CheckCompatibilityVisibility` covers parent/self disable, no-op revision, name/world edits, explicit hidden state and typed edits. | Closed by independent re-review. |
| IR-03 / P2 | Native v4 unknown node payloads became Group nodes; unsupported camera projection fields could also be ignored. | An opt-in reflected-record strict-field flag is enabled only on node and camera records. Other records keep normal unknown-field warnings; registry identity includes the flag. `CheckNativeNodeKinds` covers unknown/misspelled payload and projection rejection with paths. `CheckStrictUnknownFields` covers default compatibility, registry conflict and unchanged destination on failure. | Closed by independent re-review. |

`Q-01` is an explicit low-level boundary, not an unresolved confirmed defect: BuildViews accepts trusted derived families, including CSM projections, with the original resolved frame. Scene-bound application code uses request/seed pipeline entry points; high-level explicit-view builds reject bound scenes. The boundary is documented in SceneManagement.md.

`N-01` is a separately reproduced baseline issue: duplicating a still-loading model loses its pending whole/section material selections. The baseline already contained the same behavior. It is recorded and not repaired under this change's rule to fix introduced or directly blocking issues (tasks 11.4); it is not represented as passing or silently added to the current scope.

## Validation

- Debug build: exit 0 (`FixDebugBuildFinal.log`). The first build failed because the new test referenced a serialization API outside its target dependencies; the test was corrected to exercise the existing reflected archive entry without adding a module dependency. The failed log is retained.
- Release build: exit 0 (`FixReleaseBuild.log`).
- Debug targeted regression: 19/19, 65.86 seconds (`FixDebugTests.log`, detailed raw log alongside it).
- Release targeted regression: 19/19, 31.49 seconds (`FixReleaseTests.log`, detailed raw log alongside it).
- These 19 include the required model fixtures plus reflected archives, Scene management/runtime/Viewer, native/shared publication, material contracts/bindings/rendering, frame ownership/pipeline and real Forward/Deferred/CSM/model/shared-asset rendering. Existing renderer tests retain validation-error assertions.
- Reviewer independently ran CPU reflected_archives and scene_management: 2/2, 0.39 seconds (`ReviewerCpuTests.log`). It read and checked the main agent's GPU test source/logs; those GPU results are not described as independent reruns.
- Style, naming, boundaries, change OpenSpec strict, all OpenSpec strict and tracked diff whitespace checks: all exit 0. Commands, durations and raw logs are in `AuditCheckResults.json` and corresponding `Audit*.log` files.
- This audit ran relevant targeted regressions, not another full 62-test suite. The earlier full Debug/Release 62/62 and image/performance artifacts in the implementation handoff refer to the pre-audit binary snapshot. The excluded A/B matrices and captures were not rerun after these repairs.

## Evidence and delivery boundary

Evidence directory: `out/SceneOwnedCamerasAndLights/Audit/`.

- `StartingSnapshot.json`: original 122-path boundary and hashes.
- `InitialReview.md`, `AuditProbe.cpp`, `AuditProbe.log`: context-isolated initial review and baseline reproduction. `MainConfirmation.log` records the main agent's independent execution; its generated assets were moved into `MainConfirmationArtifacts/`.
- `FixSnapshot.json`: frozen repaired source, 14 paths changed since initial review (9 production source/headers, 4 tests, 1 document).
- `FinalReview.md`, `FinalReviewSnapshotCheck.json`: independent closure and 122-hash zero-drift check.
- `FinalSnapshot.json`: final complete file inventory/hashes including this audit report and updated handoff/task records. Source hashes remain those of the focused reviewed snapshot.

The audit closes the current implementation review gate with the stated exclusions and baseline issue recorded. Task 11.4 remains incomplete for the deferred validation. No staging, commit, archive, new user-owned task, or automation was performed.
