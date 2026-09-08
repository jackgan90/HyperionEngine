## Context

`FRenderSession::BuildViews` collects, culls, sorts and resolves materials on Render, then asks RHI 0 to build one packet per item. `FDrawPacket` lacks an instance count and D3D12 always draws one instance. Materials already provide immutable typed snapshots, reflected layouts, scope-aware evaluation, shared bindings/PSOs and fenced constant slices. Model groups have all-or-nothing draw preparation. Main Scene remains Renderer/RHI independent.

The owner approved implementation after artifact generation. Built-in shaders may be changed to consume instance records directly; transparent reordering and persistent slot indirection are not required. The existing lowercase boolean naming rules take precedence over the older uppercase-boolean wording in OpenSpec context.

## Goals / Non-Goals

**Goals:** Render-owned extensible batching, actual-resource compatibility rather than model/material identity restrictions, differing numeric instance values, compact visibility-aware caching, exclusive output coverage, capabilities, built-in shader migration, Scene Viewer A/B controls and measured results.

**Non-Goals:** Static geometry merging, content deduplication across imported copies, bindless/multidraw, automatic arbitrary shader transformation, full shader metadata infrastructure, transparent/overlay reordering, GPU culling or persistent instance-slot tables.

## Decisions

### 1. Plan on Render and materialize on RHI

A batch coordinator runs after item material evaluation. Engine-owned strategy interfaces describe compatibility, capacity and output plans; instance batching is the first strategy. Plans contain owned immutable data and item indices into their frame snapshot. They never retain mutable primitives or native handles. RHI 0 retains authority over buffers, pipelines, descriptors, uploads and retirement. Ordinary direct resource-service clients remain supported.

Compatibility includes actual resource identity and geometry range, vertex layout/topology, shader program and physical instance layout, effective fixed/dynamic state, target/view/pass ordering domain, final texture/sampler/read-buffer values and shared numeric constants. Hashes only select candidate buckets; full equality is authoritative. Different material instances and numeric revisions can merge. Instance values are excluded from compatibility keys. Native canonicalization is mirrored by engine-owned descriptions on Render.

Strategies claim each source item at most once through a coordinator. Unsupported items remain ordinary draws. Reordering requires explicit pass permission and safe opaque/masked state; barriers prevent crossing transparent, overlay, stencil-sensitive or other order-sensitive work. Future strategies use their own compatibility policy under the same scheduling and coverage contract.

### 2. Macro-selected permutations and reflected instance arrays

CPU material passes describe which constant blocks contain a single fixed array of instance records. Preparation validates that declaration against reflection and exposes each record member as an ordinary logical material parameter. Instance stride/capacity and member offsets remain compiled program metadata, preserving existing parameter names, semantics and override precedence. Arrays/structures/matrices inside a record use existing typed packing. Undeclared shaders continue as single draws.

Model's Object and Surface numeric blocks become instance arrays; View and Scene remain shared. Triangle and GUI use an instance array for their transform, with GUI still submitting ordinary draws. Vertex shaders index using `SV_InstanceID`; pixel shaders receive a non-interpolated index when necessary. Ordinary draws compile with `HYP_ENABLE_INSTANCE=0`; batches compile the optional `Instance` permutation with `HYP_ENABLE_INSTANCE=1`. Shaders use `#ifndef HYP_ENABLE_INSTANCE` to default to zero and `#if HYP_ENABLE_INSTANCE` to select instance access. No shader metadata system is introduced. Macro text alone is not a capability test: the instance permutation must reflect a system instance-ID input and the declared record layout. Shaders that omit the contract, force zero, or fail optional compilation retain the ordinary path with a fallback diagnostic. Instance inputs must be available in the ordinary logical pass so existing material evaluation and override validation remain authoritative.

The actual draw count bounds the accessible instance records. RHI binding layouts retain the full reflected shader extent plus explicit instance stride/capacity, allowing a compact published slice sized for the current count. Pipeline validation checks the declared array, and draw validation checks count and the required accessible extent. No per-instance vertex stream is introduced. Native start-instance remains zero, keeping shader indices local to the bound slice.

### 3. Compact chunks and bounded caches

Stable identity is Scene/Slot/Generation/LocalItemId. Anonymous emitted items can batch in one frame but cannot claim cross-frame identity. The coordinator caches compatible descriptions and immutable instance data under bounded entry/byte budgets with explicit retirement. Cache matching checks actual emitted contents and effective parameter values, not only primitive revision.

Visible instances are compacted in deterministic order and split by reflected capacity and device constant limits. Unchanged membership/order/layout/numeric values reuse packed data and GPU slices. Changed membership or instance values rebuild affected chunks; shared View changes leave instance data reusable unless those values depend on View. Repacking a chunk is acceptable; persistent slot indirection is deliberately deferred. Caches do not preserve unbounded historical revisions or keep removed resources alive indefinitely.

GPU batch slices share the existing constant-page allocator and retirement mechanism. Published bytes are immutable. Cache eviction drops references; packets/lists/fences retain old pages. Failed Present, cancellation, no-frame removal and shutdown keep their current safety guarantees.

### 4. Preserve group failures and result reporting

Prepare and validate source items/groups before final publication. Batch preparation can occur provisionally, but packets publish only after all source group failures are known. If one source group fails elsewhere, remove its members from every affected batch and rebuild the surviving compact data on the exceptional path. Independent groups remain drawable. Batch-specific preparation failure can fall back to individual draws before publication; it must never submit both representations. Per-item reports preserve revision/frame/family/view/usage attribution.

### 5. Capabilities and measurement

RHI exposes usable instanced drawing. Instance counts are positive, bounded by the program, constant range, bindings and API arithmetic. Each stage's constant count, root-signature budget and device alignment remain enforced. Smaller operational chunks are allowed; unsupported contracts produce explicit failure/fallback reasons.

Scene Viewer enables batching by default and exposes a runtime checkbox plus a command-line A/B override. Existing benchmark CSV columns remain meaningful and new columns distinguish visible items, submitted instances, batches, singles, cache hits/rebuilds, bytes and Render/RHI CPU time. GPU graphics-pass scopes use existing profiling support; reported totals include clear and GUI passes and are not scene-only timing. Correctness uses identical cameras, culling settings and pixels; performance uses serial interleaved warmed runs with the same executable and configuration.

## Risks / Trade-offs

- Visibility churn copies unchanged records in affected chunks -> keep chunks bounded and report uploaded bytes; add indirection only with measured justification.
- Reordering changes coplanar/order-dependent results -> explicit opt-in and strict ordering barriers; test transparent/stencil/overlay fallback.
- Reflection of arrays differs across DXIL/SPIR-V -> validate physical record layout and preserve per-format reflection tests; native execution remains D3D12.
- Cross-group batching weakens atomic failures if published too early -> provisional preparation followed by one final publication phase and targeted mixed-group failure tests.
- Fewer draws can be offset by CPU planning -> cache effective descriptions/data, retain the existing material evaluation caches and measure planning separately.

## Migration Plan

Land contracts and tests, migrate built-in shaders and count-one consumers, integrate Render plans and GPU cached data, add Viewer controls/metrics, then run CPU/GPU regression and repeated A/B measurements. The runtime switch provides immediate comparison and fallback to count-one rendering. Existing serialized keys and executable/target names remain stable.

## Open Questions

No owner decisions remain open. Operational cache budgets, chunk capacities, conservative eligibility and measured evidence are recorded in `docs/InstanceBatching.md`, `docs/InstanceBatchPerformance.md` and this change's `implementation.md`.
