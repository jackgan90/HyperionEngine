# Validation record

Validated on 2026-09-29 using Windows / D3D12 Debug. The implementation and audit records below describe the state before the subsequently authorized archive and local commit.

## Implementation

- `FPlacementService` owns common candidate conversion, asynchronous model preparation and document commit logic. Presets and content model drops share viewport routing, positioning, preview and history.
- `scene.placement.place_model` uses reflected model references and Main polling. Both initial request admission and scene-work publication are followed by current-document/revision checks.
- Asset saves cancel gestures and clear preview caches. Invalidated unused scene registrations remain available to history, while new requests prepare current data.

## Build and checks

- `tools/Build.ps1 -Preset debug`: passed. Final `-Target hyperion_editor` rebuild passed after adding the revision recheck; no C++ compiler warnings or errors in the successful build logs.
- `python tools/CheckStyle.py`: passed for all owned source formatting and paths.
- Repository `CheckStyle.check_naming` with the Debug compile database: passed for all 15 changed/new C++ translation units, including clang-tidy and boolean-prefix checks.
- `python tools/CheckBoundaries.py`: passed.
- `git diff --check`: passed.
- `openspec validate --all --strict`: 99 items passed.

## Regression coverage

The expanded CTest run passed 12/12: `automation_scene`, `editor_content` (fixture), `editor_content_transition`, `editor_state`, `automation_contracts`, `scene_runtime_instance`, `plugin_applications`, `gui_input_and_data`, `object_placement`, `automation_capability_parity`, `editor_placement`, and `editor_model_placement`.

After the final revision recheck, the three directly affected tests (`automation_capability_parity`, `editor_placement`, `editor_model_placement`) passed again, 3/3.

`editor_model_placement` uses real GUI input on Content Browser tiles and verifies cold release, two-instance preview, outside/re-entry, repeated drops, Escape, outside release, changed camera context, incompatible/corrupt/missing-dependency assets, busy automation rejection, node naming/selection/focus, shared model data, Undo/Redo, unused-reference exclusion, and save/reopen. Live CLI and MCP sessions verify discovery/schema, actual placement, source instances, stale document/revision rejection, failures without node/revision changes, selection/history and save. `automation_scene` also verifies a registered operation with no placement provider returns unavailable.

`scene_runtime_instance` includes a saved, previously unused registration regression that checks new placement receives the updated internal model transform. Existing preset tests still exercise all nine entries and their cancellation paths.

The generated preview screenshot was visually inspected: both model instances are visible during a held viewport drag. GUI reports and shutdown report zero GPU validation errors.

## Evidence and limits

Local logs are under `out/`: `ModelPlacementBuild.log`, `ModelPlacementRegression.log`, `ModelPlacementFinalBuild.log`, `ModelPlacementFinalTests.log`, `ModelPlacementNaming.log`, and `ModelPlacementSpecValidation.log`. Desktop fixtures and screenshots are isolated under `out/editor-tests/model-placement-*`.

Verification used deterministic native fixtures and the existing regression assets. Release builds and prolonged large-model memory stress were not run. Scene-scoped preparation caches intentionally survive cancelled gestures until scene close; they are excluded from persistence unless referenced by a scene node.

## Source material preview follow-up

Native Content Browser previews now select ready source materials per section, with the existing shaded material as a preparation/upload fallback. Presets retain the original shaded preview. Source-material items enter ordinary non-shadow view snapshots before material preparation, pass selection and transparent sorting; the moving snapshots are not retained as scene view caches. They receive scene lighting and contribute viewport depth to screen-space effects but do not register persistent scene primitives or shadow-map casters. The renderer helper and existing shared placement service require no automation schema or operation changes.

- Full Debug build passed after retrying one transient LNK1168 write lock on the new test executable. The successful build contains no compiler warnings/errors. The added `transient_material_tests` target is also a `hyperion_check` dependency; final target configuration/build passed.
- Source formatting/path checks passed (925 files); boundaries passed (888 files / 38 modules); semantic naming passed for all 21 changed/new C++ translation units.
- Core tests passed 4/4: `transient_materials`, `render_resources`, `editor_placement`, `editor_model_placement`.
- Shared-path regressions passed 6/6: `plugin_applications`, `deferred_rendering`, `selection_outlines`, `instance_batching`, `scene_rendering`, `automation_capability_parity`.
- `render_resources` holds texture uploads incomplete while another section is ready, verifies simultaneous shaded/source selection, then releases upload and verifies automatic switching, immutable earlier snapshots and cached ready selection without extra upload.
- `transient_materials` compares full-frame preview and committed-node images (maximum per-channel tolerance 1/255) in Forward/Deferred and Standard/Reversed depth. It covers source color, textured alpha masking, transparent ordering against an existing transparent object, lit directional-light shading, movement and cancellation. Shadow maps are disabled for this image-equivalence fixture because previews intentionally do not cast shadow maps. GPU validation errors are zero.
- `editor_model_placement` additionally asserts both internal preview instances use their section's original render-material handles. The GUI report has `model_placement_verified=true`, `failed_models=0`, `validation_errors=0`; preset and live CLI/MCP placement tests pass.
- OpenSpec strict validation passed for all 99 items; final active-change validation and `git diff --check` passed. No staged files, archive or commit.

Follow-up logs: `out/ModelMaterialBuildFinal.log`, `out/ModelMaterialCheckTarget.log`, `out/ModelMaterialTests.log`, `out/ModelMaterialRegression.log`, `out/ModelMaterialNaming.log`, `out/ModelMaterialSpecValidation.log`. The GUI fixture is `out/editor-tests/model-placement-bo7v9b1y`.

Release and large-scene preview performance were not benchmarked. While source previews are active, non-shadow view snapshots are prepared afresh so movement and material transitions cannot reuse stale prepared items; persistent scene registrations and GPU resources remain shared.

## Quality audit and repairs

Two independent reviewers, created without conversation history, reviewed all staged/unstaged/new task files against baseline `2ab5bb77f6ccaab6a841e7c06727c5d003a0492a`. The initial 38-file snapshot and later 41-file repair snapshots are under `out/ModelPlacementAudit/`. No unrelated user edits were identified. Review covered the shared placement service, GUI/automation adapters, cancellation and publication lifecycle, material readiness, view preparation, sorting, ownership, cache behavior, tests and documentation. Source files were frozen while reviewers read them.

### Confirmed findings and disposition

- **MP-01 / P2 — preview-to-node handoff:** the release frame could include both a ready transparent preview and its formal geometry; nonempty `GetDrawResults()` could also remove pending-material fallback before any ready draw. Main independently verified both paths through `CommitPlacement`, the second Main scene tick, `EditorFrame`, and default entries in `FModel::GetDrawResults`. The repair adds exact primitive replacement handles to frozen transition snapshots, filters formal items from affected views, refreshes per-section selection each frame, and retires preview plus exclusions together when the current model publication is ready. Shadow depth suppresses replaced formal items without adding preview casters. Both original triggers are closed by targeted independent re-review and regression tests.
- **R-R01 / P2 — readiness failure boundary introduced during repair:** querying the publication token before checking initial/model/error state could throw from the new readiness query. Main verified `GetToken` preconditions and the Editor polling order. The query now checks scene/bridge errors and model readiness first; Editor polling/freezing retires transitions on scene errors. Tests exercise a new unpublished instance, loading/ready/failed/stale handles, and an actual rejected publication. Targeted re-review found no remaining blocker.

The generic asset payload's outside-release handling was investigated but not confirmed as a current defect: only native Model candidates own this gesture, and the current Details panel has no writable Model-reference target. No speculative behavior change was made. Extra queued-graph image tests, full shadow/contact-shadow image cases and large-scene timing remain optional coverage; source ownership and shadow filtering were reviewed, and replacement filtering has a direct shadow-view assertion.

### Final validation

- Full Debug build passed (`VerifiedBuild.log`). A newly added test initially called a nonexistent `FSourceModel::Flush`; this test-only compile error was corrected before the successful build. No compiler warnings/errors appear in the final successful build log.
- Final CTest run passed **10/10**: `scene_runtime_instance`, `plugin_applications`, `selection_outlines`, `transient_materials`, `instance_batching`, `render_resources`, `scene_rendering`, `automation_capability_parity`, `editor_placement`, `editor_model_placement` (`Tests.log`).
- `transient_materials` now also renders persistent geometry and its replacing preview together and compares against the single committed image across Forward/Deferred, both depth conventions and opaque/masked/blended materials. `render_resources` tests mixed upload readiness, exact replacement and shadow exclusion while preserving older snapshots. GUI acceptance asserts any retained publication preview excludes the actual formal handles on the real release frame.
- Final formatting/path checks passed for 925 owned files; semantic naming passed for 23 affected C++ translation units; module boundaries passed for 888 source files / 38 modules. OpenSpec strict validation passed all 99 items. `git diff --check` passed.
- GUI fixtures: `out/editor-tests/placement-fmwbllp5` and `out/editor-tests/model-placement-jhjh3ogn`; model placement and GPU validation checks passed. Live CLI/MCP discovery and invocation remained equivalent to GUI placement.

Independent source reports: `out/ModelPlacementAudit/PlacementReview.md`, `RendererReview.md`, `PlacementRereview.md`, `RendererRereview.md`, `PlacementFinalReview.md`, and `RendererFinalReview.md`. The last renderer report verifies the final test-only correction against `VerifiedSnapshot.json`; the other reviewed source is unchanged. Reviewer conclusions are source audits; the build/test executions above were run separately by Main. Final documentation adds the verified handoff contract and this evidence record.

No unresolved confirmed findings remain in the audited scope. Release, prolonged stress and large-scene performance remain unverified. At audit completion, the change was active with no staging, archive, commit or push.

## Archive delivery

Following explicit user authorization, this change was archived on 2026-09-29 and its six requirements synchronized into `openspec/specs/content-model-placement/spec.md`. Duplicate handoff scenario/design paragraphs were removed and the generated main-spec Purpose completed. Runtime, Editor and test sources retain their verified audit hashes; no additional build or GPU run is needed for these documentation-only archive edits. Commit scope includes the reviewed implementation, documentation, archived artifacts and synchronized main spec, excluding local `out/` evidence. Push is not part of this delivery.
