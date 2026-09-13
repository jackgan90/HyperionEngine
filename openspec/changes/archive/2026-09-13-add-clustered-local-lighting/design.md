## Context

The scene pipeline already publishes typed local lights and supports conservative BVH queries, shared BRDF evaluation, HDR Forward and Deferred volumes. Graphics shader reflection/material bindings support structured read buffers; compute stages, UAVs and graph buffer writes are absent.

## Goals / Non-Goals

**Goals:** Default clustered point/spot lighting across opaque, masked and lit transparent receivers; identical per-light evaluation; retained opt-out behavior; immutable queued data; measurable and bounded resource use; unchanged authored Sponza appearance.

**Non-Goals:** Compute/UAV infrastructure, local shadows, other light types, depth-prepass-dependent occupancy, normal-based light rejection, Scene schema changes, commit or archive.

## Decisions

1. CPU builds a regular viewport XY grid (initially 64 pixels) and 24 logarithmic view-depth slices. Shader derives XYZ from pixel position and positive camera-forward depth, flattens X + Nx*(Y + Ny*Z), loads uint2 offset/count, then indexed light attributes. Standard/reversed Z share linear depth; sub-viewports and incomplete tiles have explicit bounds. Perspective camera metadata is required when nonempty cluster lighting is built.
2. Reuse FLocalLightIndex and the existing visibility contract for broadphase. Cluster assignment uses conservative light bounds projected onto screen tiles with near-plane-safe coverage and a conservative depth-slice interval. The Cartesian range retains false positives; exact pixel attenuation rejects them. Tighter per-cell sphere/cone tests are deferred until workload evidence justifies their cost. False negatives and duplicate indices are forbidden. Transparent receivers use the full camera depth domain without opaque depth occupancy.
3. Three immutable structured read buffers contain 64-byte point/spot records, 8-byte headers and 4-byte indices. Numeric view constants are separate. Count/prefix/fill construction has checked allocation budgets and no per-cell truncation. Empty bindings use valid sentinel storage with zero counts. Deterministic order follows existing candidate order.
4. Renderer owns caches; published scene values remain unchanged. Cache comparisons distinguish light attributes from assignment geometry, viewport/projection/pose and candidates. Unchanged data reuses sources. Even after assignment is recomputed, exact header/index byte comparison preserves unchanged sources. Changed data creates immutable generations; scopes and normal GPU fences retire old buffers and binding sets. Static material preparation uploads sources but leaves descriptor-table publication to actual draws, avoiding pinned tables for unused passes. No native backend calls leak into the algorithm and no GPU idle is introduced.
5. Cluster data binds through View semantics to HDR model materials and explicit parameters to fullscreen passes. Shadow/base/display permutations do not evaluate clusters. Built-in persisted PBR descriptions and importer revisions are updated together; renderer supplies safe disabled defaults to old standalone callers.
6. Deferred with effective directional radiance uses one fused directional/cluster/environment/emissive fullscreen pass. Without directional radiance a dedicated cluster/environment/emissive permutation avoids directional shadow sampling. Cluster-enabled Deferred never emits local volumes. HDR Forward and the shared transparency stage evaluate the same cluster list. Unlit and shadow visualization remain excluded.
7. Extract a pure single-light function returning zero outside range/cone (never discarding the entire fragment from inside a list). Reuse existing attenuation, BRDF and per-light bounds. HDR accumulation order/half-float rounding can differ from volumes; image acceptance uses tight numerical tolerance rather than requiring bit identity.
8. A default-true pipeline/config flag with CLI and GUI overrides selects clusters. Disabled Deferred preserves volumes; disabled Forward/transparency has zero local contribution. Toggles freeze per frame and do not recreate unrelated targets. Statistics distinguish algorithm, occupied cells, list references, maximum list length, construction time/bytes and volume draws. Fused pass timings are not reported as isolated local-light GPU cost.

## Risks / Trade-offs

- CPU assignment and upload-heap reads may dominate dense moving scenes → limit assignment to covered tiles/slices, cache stationary data, measure dense and moving workloads and report limits.
- Near-plane crossing and slice boundaries can omit lights → conservatively expand bounds and include adversarial geometry/GPU tests for both depth conventions.
- Long-lived material owners can retain transient buffers → use generation owners for dynamic resource sets, test stationary and moving retirement.
- Existing native PBR descriptions lack new semantics → invalidate affected import products and test old standalone material paths with defaults.

## Migration Plan

Scene point/spot assets are unchanged. Missing application setting defaults to clusters enabled. Rebuild affected native model/material import products. The runtime flag restores the previous algorithm. Keep all work uncommitted.

## Open Questions

None blocking; grid tuning and numerical/resource thresholds will be recorded from actual validation.
