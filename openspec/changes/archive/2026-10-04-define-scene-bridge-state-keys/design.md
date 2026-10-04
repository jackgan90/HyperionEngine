## Context

`FSceneRenderBridge::GetStatusRevision()` and `GetModelStatusRevision()` currently return a pair of the selected bridge counter and `FRenderResourceService::GetPublicationRevision()`. These are not structure/content revisions: scene-only publications advance the general counter, while model preparation/receipt events advance the model counter. SceneInstance caches the latter pair to avoid rescanning model readiness after camera/light-only edits.

The bridge's private material version vector starts with a model-level snapshot identity/revision, then appends a section-number/zero pair and a snapshot pair for each section. Editable material dependencies separately pair a shared instance with its observed revision. Authored changes already pass through Scene change masks; polling editable instances finds material-only changes without a logical scene revision.

## Goals / Non-Goals

**Goals:**
- Name status components according to their actual producers and preserve equality behavior.
- Give model surface, section selection and editable-instance dependencies distinct records.
- Keep no-op flushing, material-only refresh, shared freezing and atomic publication equivalent.
- Verify the runtime behavior rather than only testing defaulted record equality.

**Non-Goals:**
- Change revision increments, selection precedence, errors, readiness, counters or invalidation policy.
- Change material/scene serialization, reflected APIs, automation, renderer algorithms or GPU ownership.
- Add a general revision framework, move Renderer state into Scene, or refactor unrelated caches/pairs.
- Change plugin composition or expand the accepted maintainability checkpoint.

## Decisions

### One named status snapshot for the existing two-component contract

Declare `FSceneBridgeStatusRevision` in SceneBridge's public header with `BridgeStatus` and `ResourcePublication` counters and defaulted value equality. Both queries expose the same component meanings; the existing method names select all-publication versus model-only bridge events. SceneInstance stores this named value. Counter initialization, increment sites, Main-domain checks and resource acquire-load remain untouched.

Separate wrapper types for every revision counter would add conversions without a consumer needing that distinction. A pair alias would retain positional semantics. The named record makes the actual dependencies visible with minimal source migration; this C++ return-type change requires rebuilding consumers but has no wire or persisted representation.

### Separate material roles and retain canonical section order

Keep bridge-only records private to the class:
- A snapshot revision names `Identity` and `Revision`, retaining the existing zero-initialized value for an absent selection.
- A section revision names `SectionIndex` and its snapshot revision.
- The complete selection revisions contain one `Surface` revision and a vector of section records.
- An editable dependency names its shared `Instance` and observed `Revision`.

The vector is derived from the existing ordered `SectionSurfaces` map, so each section's identity is explicit and ordering remains canonical. This preserves exactly the distinctions encoded by the old vector without introducing a second map or changing valid/empty selection behavior. Defaulted equality compares every existing semantic input. Authored overrides and source changes remain covered by the existing Scene change-mask gate; this revision record does not replace that gate.

Reuse one frozen-material map across the complete preparation batch as before. Carry the named selection revisions and shared editable dependencies from pending work to attachments only at the existing commit point. Do not alter reserve/preallocation, admission, error recovery or retirement ordering.

### Focused behavioral verification

Capture a successful Debug baseline for `scene_rendering` and `scene_runtime_instance`. Add small bridge regressions using the existing GPU fixture/session for idle equality, metadata-only versus model status, independent resource-publication changes, material-only revisions, section reassignment/removal and detached editable dependencies. Keep section/material checks based on expected published snapshots and model-preparation counts. Existing tests continue to cover failed preparation atomicity, retained frames, shutdown, removal and reattachment; SceneInstance's status-cache regression covers the consuming cache.

## Risks / Trade-offs

- A plausible but wrong status name could hide the resource dependency -> verify named fields against the resource service and independently trigger resource completion.
- Grouping records could omit section identity or accidentally reorder selections -> preserve map iteration order and verify routing after section changes with distinct material identities.
- Moving editable references could change lifetime or polling behavior -> retain shared ownership and the exact prepare/commit/remove sequence; test removed dependencies and existing retirement paths.
- New equality might over-invalidate or miss material-only edits -> check idle preparation counts, shared updates, section updates and existing status-cache behavior.

## Migration Plan

Update the return type, private records and all repository consumers together. Build affected consumers and run the focused regression suite, style/boundary checks and independent quality audit. Stop for user diff acceptance before archive/commit. No data migration is needed; reverting the focused change restores the pair representation without changing content.
