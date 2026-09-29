## Context

Content Browser emits `Hyperion.ContentAsset.v1` paths used by typed property pickers. Viewport placement accepts only `Hyperion.PlaceableObject.v1`. Renderer already owns generic gesture snapshots, ray placement and transient geometry. Editor prepares preset models through SceneInstance and commits through FSceneEditDocument. SceneEditing exposes IScenePlacement to automation.

## Goals / Non-Goals

**Goals:** Share candidate preparation, validation, preview and commit between presets and native models; preserve history, selection, source materials and lifecycle; expose equivalent typed automation.

**Non-Goals:** External source-file import, multi-file drops, hierarchy expansion, surface rotation/grid snapping, scene format changes, new plugins, archive or Git commits.

## Decisions

1. Convert a registered preset or a validated indexed model reference into an owned candidate with a stable key, label, detached node, optional model reference and icon. Keep dynamic candidates outside the preset catalog. Reuse the existing generic asset payload so Details drop targets remain compatible.
2. Centralize UI-independent model preparation and node creation in an Editor-owned placement service. SceneEditing owns reflected requests and CPU contracts; Renderer owns runtime model/render resources. GUI handles gesture admission and positions; automation handles idle/revision admission; both use the same preparation and commit. This avoids duplicating business rules or incorrectly rejecting the GUI's own active gesture as busy.
3. Model references are normalized and resolved by the asset service. Cache keys preserve complete requested reference identity, while prepared data changes invalidate preview resources. Only models entering the placement path are requested. Existing scene-level load deduplication remains authoritative.
4. Preserve FViewportPlacementSession context checks and preset placement math. Native model previews include every internal instance at its authored transform. Commit creates one root model node named from the asset filename, retaining model materials and unit scale.
5. Loading is asynchronous. Before readiness, show status and prohibit commit. Releasing without a valid preview cancels. Esc, right click, lost focus, invalid viewport/modal state, changed document or viewport context cancel. Generic asset payloads are cancelled only when owned by the current placement session. Scene/content shutdown clears candidates/resources through the existing lifecycle.
6. Add `scene.placement.place_model` with document, revision, model reference and explicit world pivot. Keep `scene.placement.place` and its schema unchanged. Poll copied requests on Main; stale/busy validation occurs before effects and again on subsequent polls. No transport-specific domain code.
7. Cancellation does not create nodes or history. Existing SceneInstance load caches can survive until scene close, but Snapshot persists only used references. Keep this bounded to assets actually attempted; measure repeated placement memory without introducing a second cache eviction system in this feature.
8. Native model previews select each section's source material once its render material is Ready; otherwise they use the existing shaded fallback. Geometry readiness does not imply material GPU readiness. Preset previews retain their independent shaded material. Transient geometry separates overlay preview items from source-material scene items. Source items join the ordinary non-shadow view snapshots before material preparation and sorting, sharing scene lighting, opaque/masked/transparent passes and resource lifetimes. These moving snapshots bypass retained view preparation while the underlying scene registrations remain unchanged; cancellation immediately restores the ordinary cached path. No scene nodes, visibility registrations, shadow-map casters or history are created for previews. Screen-space effects consume ordinary viewport depth, including opaque/masked preview surfaces.

9. Preview-to-node publication has one display owner per frame. While formal bindings/materials are pending, freeze a fresh transition preview with current per-section readiness and the exact persistent primitive handles it replaces. Suppress those handles in every affected view, including shadow depth, and remove both the preview and exclusions once the model is ready in the current logical publication. Readiness queries return false before initial publication and on controlled publication errors. Node removal, scene revision/transform changes and load/publication errors retire the transition.

## Risks / Trade-offs

- Arbitrary models load more slowly than prewarmed primitives → explicit loading feedback and no delayed creation after release.
- Assets may change while preparing → validate reference identity and invalidate prepared preview data when source snapshots change; document/root replacement cancels ownership.
- Resource failures must not poison unrelated objects → report candidate-specific errors and preserve preset availability.
- Existing support-point positioning is not automatic pivot recentering → retain authored pivots and existing positioning semantics; test offset bounds and multiple transformed instances.
- GPU resources outlive gestures → retain immutable preview snapshots through publication and existing fence-managed resource lifetimes.

## Migration Plan

Refactor shared placement first, add model payload and automation adapters, then validate existing presets and new native model workflows. No persisted migration is required. Keep artifacts active and changes uncommitted.

## Open Questions

None required for the agreed first version.
