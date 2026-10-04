## Context

`EditorViews.cpp` derives culling and outline values from combo indices. `EditorViewportService.cpp` converts them through enum ordinals, while `ValidateViewportOptions` separately assumes numeric ranges. The enums already belong to Renderer (`ESceneCullingMode`, `EOutlineOverlapMode`). Existing RasterOptions helpers demonstrate selection by identity instead of presentation position.

## Goals / Non-Goals

**Goals:**
- Give each supported culling/outline choice one explicit identity-to-wire mapping.
- Make combo ordering and labels independent of behavior.
- Keep GUI and automation on `ISceneViewport::SetViewportOptions`, including preflight and unsupported-control behavior.
- Preserve the complete current viewport/API schemas and temporary-state semantics.

**Non-Goals:**
- Change numeric DTO field types, profiling HUD masks, visualizer/shadow/configuration contracts or rendering algorithms.
- Move Renderer enums into a lower-level module, create new modules, or change plugin composition/lifetimes.
- Refactor SceneBridge, split unrelated legacy files, or commit the maintainability draft.

## Decisions

### Renderer owns discrete viewport choices

Add a small public `ViewportChoices` contract beside the existing enum owners. Named option records associate an enum identity, fixed wire value and label. Canonical tables own the mappings; parsing, encoding, supported-value checks and presentation validation use them. Existing generic RasterOptions presentation helpers can select these records by identity without introducing a new helper framework.

An Editor-only table would leave shared service conversion and validation duplicating the mapping. Moving these enums to RasterOptions would expand the dependency/API scope without a Config consumer. Renderer already owns both choices and publicly depends on RasterOptions, so retaining that ownership is the narrow change.

### Preserve the numeric boundary

`FSceneViewportOptions` remains its existing optional-uint32 DTO. Culling wire values remain 0=None, 1=Linear, 2=Bvh; outline values remain 0=Union, 1=PerObject. Explicit lookup rejects unknown numbers before narrowing to an enum. The Editor uses parse/encode helpers for state responses and commits, and shared preflight tests supported values through the same canonical table. The existing invalid-argument message and unsupported-before-invalid precedence remain intact.

Keep record IDs, versions, member order, optional/null behavior, integer ranges, descriptions and operation metadata unchanged. Descriptions remain the current versioned external prose rather than following GUI relabeling. Changing internal fields to reflected enums now would alter schema shape, so that broader migration is deferred.

### GUI selection remains a thin adapter

Build labels from option records, find the selected index by identity, then translate the chosen record back through the explicit wire mapping. The call to the shared viewport service remains unchanged. Split the existing long popup routine only at its discrete-choice drawing boundary, so the touched routine stays within the function-length principle. Add semantic row observations through the existing acceptance interface for real GUI verification.

### Verification uses independent expectations

Capture current operation/type JSON snapshots from the baseline build before editing production code. CPU tests fix the legacy numeric meanings independently of the tables, exercise reversed/relabelled presentations, and reject unmapped values and ambiguous presentations. Extend the existing render-controls acceptance with actual GUI culling/outline selections and live automation calls; compare complete option snapshots, document revision/history/dirty state and unaffected render-settings revision. Compare post-change API snapshots byte-for-byte with the baseline.

## Risks / Trade-offs

- Numeric fields remain untyped at the external DTO boundary → confine conversion to explicit mappings; wider typed-state work is a separate checkpoint.
- GUI tests can accidentally assert only the same mapping they exercise → use fixed wire/enum expectations plus reordered/relabeled unit fixtures.
- New source registration can miss test-enabled/disabled ownership → keep CPU implementation in Renderer and GUI acceptance implementation in the existing BUILD_TESTING branch; no optional provider dependency is added.
- Table labels and external descriptions serve different presentation contracts → preserve the external schema snapshot and do not derive behavior from either text.

## Migration Plan

Land the contract, adapters, regression coverage and documentation together after independent quality audit and user diff acceptance. No data migration is needed. Reverting the focused change restores the previous internal conversions without altering saved content.
