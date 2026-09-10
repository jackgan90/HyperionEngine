## Context

CPU FScene already owns generation-safe Main model records; FSceneRenderBridge owns FModel attachments; Render owns FRenderScene and BVH collection. SceneViewer currently combines their lifecycle with manifest requests, preparation tasks, camera and selection. FRenderGraph currently wraps FPassCommands, implicitly imports swapchain color/default depth, serializes every entry, and expands deferred callbacks into passes on RHI 0. MaterialDraw infers output color count from a view's DepthTarget.

## Goals / Non-Goals

**Goals:** A reusable Renderer scene entity; explicit and validated attachment/resource contracts end to end; unchanged SceneViewer and ModelViewer behavior, image ordering, material/batch reuse, threading and lifetime guarantees.

**Non-Goals:** World/Actor/ECS, runtime parenting, new rendering algorithms, arbitrary offscreen color targets, MRT/MSAA/resolve, compute, multiple queues, transient aliasing, automatic pass culling or shader redesign. No user-facing schema/CLI migration.

## Decisions

### Scene ownership

Introduce Main-owned FSceneInstance in Renderer with private implementation, composing FScene, FSceneRenderBridge, manifest/model load state and instance-to-asset association. It exposes Load, Tick, Close, generation-safe model operations and structured loading/render readiness/errors. Bridge remains the sole owner of FModel attachments. SceneViewer owns the entity and retains only camera/input/selection/GUI/demo state. Keep the existing fixed-step demo animation unchanged; Tick advances asynchronous lifecycle and synchronization without imposing gameplay policy. Close cancels requests, joins every admitted preparation, then removes scene attachments before session teardown. Loader registration belongs to runtime integration, not the plugin. CPU Scene remains independent of Renderer and RHI.

### Explicit graph and RHI attachments

Use graph-local typed handles with ownership validation and a resource table. Explicitly import current-frame backbuffer, frame depth and sampled D32 textures, including initial/final state and content validity. A graphics pass declares zero/one color attachment, optional depth/stencil attachment with independent load/store and clear operations, render area and sampled reads. Imported resource resolution may occur on RHI 0, but identities and accesses are declared beforehand. RHI receives resolved attachment references and explicit transitions; missing depth cannot mean use an implicit default, and missing color means no RTV.

An attachment view carries format/sRGB independently of physical texture identity. Preserve mixed-sRGB draw segmentation and its order. Compile output can contain several command batches for one logical graphics pass, but only the first performs its load/clear and only the last its store. Target compatibility comes from the pass attachment signature, never the presence of a depth texture on a camera view.

### Compilation and content lifetime

Build RAW/WAR/WAW dependencies from accesses in declaration order and retain explicit After edges for external ordering. Stable topological order preserves unconstrained declaration order without inserting a universal previous-pass edge. Reject foreign handles, duplicate identities with conflicting import descriptions, invalid aspects/formats/dimensions, cycles, undefined loads/reads and simultaneous sampled/write bindings. Track content validity separately from resource state. Clear initializes its declared region; Discard invalidates it and does not prove full coverage. Store Discard invalidates contents after the pass. Export backbuffer as Present explicitly; preserve sampled depth final state for subsequent frames. Validate graph before frame acquisition and retain all resolved resources through submission completion.

### Separate view collection from pass declaration

View descriptions retain camera/culling/material usage and frame identity. A separate render pass description contains output attachments and shader reads. Forward pipeline explicitly declares shadow producers and forward consumer; session collection still freezes a shared family and queries each view independently. Deferred preparation only creates immutable draw batches and native resources for already declared passes. It cannot add undeclared dependencies or attachments. Preserve one Render-to-RHI frame boundary, coordinator-inline recording, peer joins and failure cleanup.

### Bounded migration

Migrate all owned graph/RHI callers and tests; do not keep an alternate legacy flag-driven production path. Existing zero/one color, D32/D32S8 and sampled D32 support is sufficient. Frame-local attachment references can resolve to existing swapchain storage; this does not require redesigning device submission or allocating new resources per frame. Preserve SharedDraws and weak cache ownership, compare full attachment identity/signature when caching, and keep GPU release fence-safe.

## Risks / Trade-offs

- Broad public contract migration → compile every owned target and run Debug/Release suites, including direct-RHI users.
- Async shutdown or loading-time model edits → generation-safe handles, no stale completion publication, explicit lifecycle tests and repeated Close.
- Clear/load or sRGB segmentation changes pixels → preserve exact segment order and compare deterministic captures for both viewers, shadows, UI and ordinary/instanced paths.
- New graph compilation overhead or resource churn → preserve retained packets and resource scopes; record warmed static/moving CSM baseline and final measurements.
- Regional initialization is conservative → reject unproven whole-resource sampling rather than infer shader coverage from draws.

## Migration Plan

1. Save current build/test and viewer rendering/performance baselines.
2. Extract and test runtime scene lifecycle, then thin SceneViewer.
3. Introduce explicit RHI attachments and graph resources/access validation; migrate all callers.
4. Separate pipeline declarations from view preparation and migrate deferred GUI/preview paths.
5. Run contract, lifetime, GPU and both viewer regression suites; fix failures and update architecture/verification documentation.

Keep the change reviewable as one active OpenSpec change. Rollback consists of reverting its owned source changes; no persistent data migration is required.

## Open Questions

None blocking. Concrete type names and helper boundaries may be refined during implementation while preserving these contracts and acceptance requirements.
