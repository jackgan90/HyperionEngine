## Context

FRenderSession already serializes Render collection and RHI pass construction. FModel is currently a Main-side Renderer adapter owning bindings. Scene contains immutable assets only. The user approved flat runtime instances, manifest-based viewing and BVH frustum culling without altering primitive draw cardinality.

## Goals / Non-Goals

Goals: renderer-independent logical ownership; owned incremental synchronization; Render-only BVH and pre-collection visibility; observable, conservative results; reusable multi-model viewing.

Non-goals: parenting, animation/deformation, LOD, GPU instancing, PVS/occlusion implementations, full editing UI, git commit or archive.

## Decisions

### Logical data and rendering attachment

FScene owns FSceneModel records with scene/slot/generation handles, monotonic revisions, name, immutable asset and prepared bounds/instance metadata, world transform, visibility and material overrides. Pure data lives in Scene without Renderer/RHI dependencies. Main ownership is checked against its construction thread. Mutations create owned changes consumed by a single FSceneRenderBridge. The bridge retains Renderer FModel adapters per logical handle, observes asynchronous receipts, and never lends logical memory to Render. Keeping FModel as the compatible rendering attachment avoids breaking existing low-level clients; new logical data is named FSceneModel to avoid confusing it with the existing per-section FModelInstance.

Each bridge attaches one logical scene to one client. Change collection has explicit acknowledgement so failed admission can be retried. Initial creation carries complete transform/visibility/material state, avoiding an identity-transform frame. Removing a model uses one Render batch for all members. Frame snapshots retain resource leases after removal. Bridge flush runs every Main tick, including minimized ticks. Loading consumers verify handle generation before attaching completed assets; failure affects only that entry. Producers stop before scene/client/resources close.

### Spatial and visibility boundaries

Math owns generic bounds/frustum utilities. Immutable per-asset metadata is prepared off Render and shared by model instances. Bounds use column-vector RH zero-to-one clip conventions, affine world transforms, conservative epsilon and fail-open unknown handling.

Each CreateBatch defines one Render culling group; standalone Create defines a one-member group. FRenderScene owns group membership, dirty bounds and an ISceneSpatialIndex. Primitive GetWorldBounds is a read-only optional conservative bound covering every possible collected item; custom primitives default to unknown. Static primitives use explicit CPU bounds or ready resource geometry; clip-space primitives stay in the unbounded path. Unknown group members make the group unbounded. Unresolved bounds are reconsidered independently of spatial visibility, so uploads cannot strand off-screen instances.

The binary BVH stores group IDs and world AABBs, uses deterministic median splits, rebuilds after topology changes, refits dirty leaves/ancestors and rebuilds when accumulated motion degrades tree cost. Unknown bounds use a separate candidate set. Queries are conservative and deduplicated. A visibility interface tests bounds independently of the index. None/Linear/BVH modes share primitive/item filtering policy; None skips spatial rejection but retains hidden/unready validation. Results belong to a view, never mutate logical visibility. GPU visibility will require asynchronous graph work and cannot be represented as a mandatory synchronous bool-only stage.

### Frame flow and stable ordering

Render applies queued commands, refreshes dirty groups, queries BVH, tests candidate primitive bounds, and calls Collect only on candidates. Existing item-level filtering and global material/depth sorting remain. Candidate primitive slots are sorted in the existing registry order before collection, preserving equal-depth ties and zero-to-many output. No draw-generation cardinality changes.

### Viewer and manifest

A SceneViewer plugin reuses the Viewer application, asset service, Scene bridge and GUI wrapper. A reflected version-1 manifest has unique asset IDs/relative paths, unique instance IDs, TRS/visibility and camera settings. Validation rejects duplicate/missing IDs, nonfinite values and invalid camera parameters. CPU loading/metadata preparation run asynchronously; cancellation drains producers. Multiple occurrences share assets/resources. ModelViewer migrates to a one-model logical Scene.

SceneViewer provides free camera/orbit-fit navigation, culling mode selection, freeze-culling view, optional bounds visualization, per-model loading/errors and runtime add/remove/move/visibility demonstration controls. Diagnostics distinguish models, visited BVH nodes, candidates, Collect calls, emitted/final items and draws, plus update/query timing and rebuild/refit counts. A checked-in manifest demonstrates repeated and distinct models; an integration fixture places many objects outside the view.

## Risks / Trade-offs

- Model-wide boxes can be loose -> keep primitive fine culling; future groups can subdivide one model.
- Topology rebuild is not optimized for per-frame bulk spawning -> batch edits and measure update time; index interface admits a dynamic tree later.
- Refit can degrade traversal -> cost-triggered rebuild and deterministic diagnostics.
- Unknown bounds increase work -> fail open, count them, and resolve ready metadata without visibility dependence.
- BVH traversal changes input order -> restore stable primitive slot order before Collect.
- Asynchronous errors/removal -> generation checks, owned snapshots, observed receipts and existing GPU retirement.

## Migration Plan

Add pure Scene/math and tests, integrate Render groups/bridge while retaining direct clients, migrate ModelViewer, add SceneViewer and run existing/new Debug and Release acceptance. Existing configs remain accepted. No automatic archive or commit.

## Open Questions

None blocking. First-version performance is measured on representative hundreds/thousands of instances; no universal speedup guarantee is asserted.
