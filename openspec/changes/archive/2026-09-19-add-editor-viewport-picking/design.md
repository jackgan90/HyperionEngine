## Context

Scene owns authoritative CPU hierarchy and transactional edits on Main. Renderer consumes a single acknowledged change stream and maintains Render-only visibility BVHs. Editor owns selection, history, gizmos and navigation. Geometry preparation already runs on Worker; the same immutable model data can serve multiple scene objects. Scene cannot depend on Renderer or RHI.

The research baseline compared Unreal Engine 5.8.2 hit proxies (local revision 16d75d84714512edfb744e1fd0a59e9c74d57873), Godot editor dynamic BVH plus cached TriangleMesh (57277407e77e61b161f35dbd7aeb510f7a9e26a6), Bevy mesh picking (80240058748cf3c7fdcd79511f0525f464405a28), and Babylon GPU picking (5c0c12d93845c42f792030c537c4579e00190d47). Godot's separation of scene candidates and shared mesh queries best fits the current CPU ownership model. Unreal's per-material hit rendering is more accurate for shader-driven surfaces but requires a separate render/readback path; Bevy's all-object bounds pass does not meet the incremental scene-index goal.

## Goals / Non-Goals

**Goals:** Accurate nearest static mesh hit on current Main state, low repeated-query cost, incremental edits, stable handles, reusable CPU APIs, optional preparation, correct input arbitration and deterministic validation.

**Non-Goals:** Pixel-identical shader alpha/deformation selection, previous-presented-frame reconstruction, GPU readback, physics, multi-selection, box selection or a new plugin backend registry.

## Decisions

1. **Share algorithms, not mutable trees.** Move the existing object bounds BVH into Math with generic bounds predicates and ray traversal. Renderer keeps its public visibility adapter, existing counters, fail-open unbounded objects and independent instances. Scene maintains another index. Ray traversal visits near children first and only shortens the distance after an accepted triangle, never merely an AABB intersection.

2. **Scene owns lazy query state.** Allocate query state on opt-in/first query. Successful Scene mutation commits invalidate changed slots independently of the render change stream. Query synchronization consumes only its own pending state; rendering acknowledgment cannot lose picking updates. Keep full Scene/Slot/Generation identity at the API boundary and validate before use. Transform changes refit bounds; membership changes rebuild; camera/light/metadata edits do not rebuild geometry. Invalid or unprepared geometry produces explicit Unavailable where a reliable miss cannot be established.

3. **Prepare immutable geometry on Worker.** An opt-in scene-instance option requests triangle acceleration during existing native-model preparation. Default consumers retain existing preparation cost. Store compact node/range arrays per primitive, shared through immutable FSceneModelData; use primitive-instance bounds before triangle queries. Do not use the dynamic object tree's map/set allocation for every triangle. Existing preparation cancellation and Close joins own lifetime. No serialized cache, Main triangle construction or global readiness gate.

4. **Define geometric semantics explicitly.** Queries normalize the world ray and bound world distance. Instance transforms combine scene and model-instance matrices; local direction remains unnormalized so the ray parameter stays in world units. Support affine transforms, including negative scale and shear; singular instances use a bounded world-triangle fallback. Respect inherited enabled state, model visibility, source node/primitive and section visibility. Resolve culling from current section/model/default material snapshot; offer an explicit two-sided policy. Return handle, instance, primitive, triangle, point and distance. Alpha discard and shader deformation remain documented limitations.

5. **Use the same view construction.** Renderer exposes a reusable viewport-ray helper based on the same resolved camera pose/lens and viewport pixel aspect as rendering. Clip against near/far planes and support both depth conventions. Editor uses its independent ViewCamera or explicitly selected preview camera; invalid preview is unavailable. Query uses current Main scene state, not asynchronous Render snapshots.

6. **Keep selection policy in Editor.** Record a potential left click on press and select on release within a small logical-pixel threshold. Gizmo capture, navigation, text/modal UI, focus loss, scene/view changes and invalid viewport cancel it. Geometric hit selects the owning scene object; confirmed miss clears selection; Unavailable preserves it. Selection creates no history command or document dirty state. Initial outliner selection is performed once, so an explicit empty selection persists.

7. **Give the viewport image widget input ownership.** The GUI adapter registers an interactive image item using the same identity as overlay capture. Ordinary left presses remain owned by the image across held frames and release normally, preventing the GUI window-background drag from making the viewport ineligible. Editor retains gesture arbitration. Tests exercise real Outliner-to-Sponza selection with a multi-frame hold as well as ordinary and overlay-owned GUI image gestures.

8. **Preserve conservative hit limits and material eligibility.** Traversal limits round accepted float hit distances upward by one representable step and clamp to the incoming limit, keeping tie candidates reachable while preserving caller clipping. Scene query options support exclusions for each material usage. Renderer supplies the scheduled pipeline query policy and shares legacy Forward exclusions with render-view construction; Editor consumes that CPU policy.

## Risks / Trade-offs

- CPU geometry differs from alpha-tested/displaced pixels → document the contract and keep a future GPU path separate.
- A single very large asset can dominate memory/build time → compact immutable triangle nodes, shared geometry, Worker opt-in and candidate/triangle counters.
- Readiness and edits race with loading → publish immutable data on Main, preserve epoch cancellation/joins, invalidate replaced data and distinguish Unavailable from Miss.
- Refactoring visibility could change culling → retain public adapter and run spatial/local-light regressions including invalid bounds.
- View/input drift → common projection construction plus targeted ray and editor interaction acceptance coverage.

## Migration Plan

Existing callers retain default behavior. Editor enables picking preparation explicitly. Introduce Math/Scene layers first, then view integration, then editor policy. No asset migration or schema change. Rollback removes Editor opt-in and integration without changing saved scenes.

## Open Questions

None blocking implementation. Performance acceptance uses counters and comparisons to exhaustive queries rather than machine-specific timing thresholds. Dynamic scene tree quality retains the existing rebuild heuristic; model-instance local acceleration can be added later if profiling demonstrates a need.
