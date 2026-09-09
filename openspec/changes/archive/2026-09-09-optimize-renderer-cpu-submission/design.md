## Context

The baseline at `144c1e5` is recorded in `out/cpu-analysis-20260909/Analysis.md` and its raw CSV/JSON/Tracy captures. Debug and Release use their existing optimization flags and enabled D3D12 validation. A 600-triangle full-engine workload spends 26.123/3.734 ms static and 49.307/7.705 ms moving. Five-view CSM motion increases PrepareMaterials and PlanBatches together by about 2.57 ms in the optimized trace. Providers already share view results; the remaining per-item resolution/table/candidate work is the principal target. Native recording has a separate draw-count slope.

## Goals / Non-Goals

**Goals:**
- Reduce camera-motion and multi-view CPU cost through generic immutable scope sharing.
- Reduce ordinary submission cost without weakening validation or GPU lifetime guarantees.
- Preserve all material/provider/override, batching/order/fallback and shadow semantics.
- Publish stable benchmark commands, build identities, operation counts and repeated A/B results including P95/P99 and image checks.

**Non-Goals:**
- Changing Debug optimization, disabling driver validation, reducing cascades/resolution/update frequency, replacing the job system, or introducing cross-frame application simulation.
- Replacing generic material contracts with PBR-specific layouts or bypassing custom strategies.
- Promising a particular FPS before measurement. The measured 600-draw Debug P95 below 16.67 ms and moving CSM CPU increment below 1 ms are engineering targets; report any remaining gap explicitly.

## Decisions

### 1. Preserve evidence and measure layers

Preserve the original build binaries before rebuilding. Add a repeatable benchmark tool with fixed 0/1/100/300/600/1200-draw scenes, static/moving cameras and default CSM, using the existing Viewer CSV path. Add a prepared-packet recording benchmark to distinguish material/scene and native recording from Present waits. Reuse HYP_PERF for stable missing phases and bounded operation counters; disabled profiling remains cheap. Timed runs exclude screenshots/readbacks, background builds and other GPU tests. Compare alternating baseline/candidate runs with identical flags, readiness and exact draw/submission coverage.

### 2. Share material refresh work by dependency and immutable value identity

Prepare per-program parameter dependency/index metadata once. Pure Global/Frame/Scene/View/Pass updates may share immutable updated values between items with the same effective source/override contract and context keys. Object/Material/Draw and mixed dependencies retain item-specific evaluation. Use bounded caches or shared sparse table pages so equal old value pages with the same updates are transformed once; never mutate previously published parameter objects. Preserve resource identity unless effective resources change. Avoid a new scope-specific public material ABI: parameter access and existing arbitrary constant-block packing continue through engine-owned tables.

### 3. Separate batching invalidation domains

Keep immutable geometry/layout/resources/state signatures distinct from shared numeric values and instance numeric values. For the built-in instance strategy, a refreshed plan may retain membership only after proving the effective compatibility partition and order are unchanged. Share refreshed compatibility data where possible; retain instance data/GPU slices when their layout, emitted member identities and values match. Arbitrary strategies retain full conservative invalidation unless an explicit contract proves independence. Perform cache retirement at the family/frame boundary instead of rescanning the whole cache for every cascade. Full-signature equality remains the collision guard.

### 4. Reuse D3D12 recording safely

Pool native command lists by fenced frame slot and recording context. Reset allocators/lists only after the slot fence completes and without mutating an externally retained logical recorded result. Clear all per-list state caches on reset. Cache PSO, topology, stencil/blend, vertex/index views and scissor in addition to existing root/descriptor binding caching. Keep dynamic arguments, resource owner/type/readiness and range validation; eliminate scratch allocations and repeated immutable checks only where a validated object or completed-fence snapshot supplies equivalent evidence. Add direct correctness tests for state transitions and reuse after cancellation/failed Present.

### 5. Reduce copies and dependent task round trips

Use explicit consuming render-graph compilation or immutable command storage while preserving existing const Compile callers. Fence retention shares command packets rather than deep-copying vector payloads. Eliminate redundant primitive state copies and per-view cache maintenance where ownership permits. Collect end-frame device statistics in the existing RHI end-frame task. Evaluate empty GUI and standalone Present transition costs with evidence; retain explicit synchronization where it is required by resource ownership or plugin contracts.

Only transparent items require projected sort depth. Compute local culling transforms only when culling is enabled and the conservative-bound contract permits it. A visible GUI-to-hidden transition must clear prepared packets once before subsequent empty GUI tasks can be skipped. A configurable benchmark camera step retains the default small orbit and permits larger visibility/CSM stress runs.

## Risks / Trade-offs

- Shared scope reuse can hide mixed dependencies or overrides → include all effective dependencies, fallback/error transitions and exact source identities; test arbitrary schemas and retained old frames.
- Grouping reuse can merge different shared values or break custom strategies → verify effective partitions and preserve conservative fallback.
- List pooling can reset GPU-owned storage or externally observed handles → gate reuse on the established slot fence and separate native reusable storage from immutable logical results.
- Larger caches can trade CPU for retained memory → explicit bounded retention, reuse/eviction counters and continuous-motion resource checks.
- Performance noise can obscure small wins → alternating repeated runs, warmup, sample coverage, raw results and stage attribution; no removal of outliers.

## Migration Plan

Implement benchmark/diagnostics first, material and batching second, native recording third, data/scheduling last. Build and test each coherent stage, then run final interleaved baseline/candidate measurements. No serialized migration or dependency change is required. Existing conservative paths remain available where a reuse proof fails. Deliver completed OpenSpec tasks and evidence; archive/commit are separate user actions.

## Open Questions

The final native recording slope and remaining CSM costs will be resolved by stage measurements. Pool size and refresh cache limits will follow observed working sets while retaining bounded behavior; no unresolved input blocks implementation.
