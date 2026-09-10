## Context

Baseline is `62bac8f`. Frozen Debug runtime is in `out/camera-motion-20260909/baseline-debug` (Viewer SHA-256 `D475887BCE242EA61C1D9E89C0CBA3602C4658C068E13F5FC6B3950815461D68`). Default Scene, 200 warmup / 400 measured frames, hidden window, GUI enabled, VSync off: static mean/P95 6.348/8.064 ms; small orbit 20.613/26.104 ms. Large orbit without GUI is 26.633/31.744 ms. These are separate conditions, not one A/B matrix.

The baseline Debug detail trace (200 frames) attributes 3.449 ms to material preparation, 2.522 ms to collection, 1.516 ms to snapshot preparation, 1.531 ms to batch planning and 3.663 ms to RHI pass preparation. Native draw recording is approximately 0.3 ms. Shared material refresh is entered about 713 times/frame although provider evaluation and constant writes are already shared. Parent/child scopes overlap; thread waits are not additional on-CPU work.

## Goals / Non-Goals

**Goals:** Reduce motion-induced preparation by removing repeated local revalidation and publication across pipeline layers. Refresh shared dependencies at group granularity. Retain useful local preparation across visibility transitions. Preserve exact source coverage, images, failures, old frames and resource retirement. Target Debug moving GUI P95 below 16.67 ms and substantially reduce the static/moving preparation delta; report measured shortfalls instead of changing the target after measurement.

**Non-Goals:** Change Debug optimization or native validation, lower shadow resolution/cascade frequency, skip input frames, alter the shader ABI, merge execution domains, or claim GPU-bound gains from a CPU optimization.

## Decisions

1. Publish immutable shared-update contracts after ordinary material evaluation proves separability. The contract contains the compiled pass, shared parameter indices/dependencies and weak expected resource values; it must not retain an unrelated object's scopes. Renderer-owned stable collection revisions permit a retained item to use this contract directly. Missing/default transitions, changed resource values/dependencies, Object/View mixed providers and Draw dependencies return to complete evaluation. A per-view update table evaluates each compatible contract once, with a bounded cache.
2. Distinguish complete prepared contents from stable local draw structure. A reusable local proof requires unchanged scene/resource publication, ordered source identities, unchanged local parameter identities, shared updates that cannot affect instance payload, compatible view/pass/target state and the built-in batching strategy. Use that proof to reuse batch membership and RHI admission, while refreshing actual shared constant bindings and current receipts. A mere matching hash or camera revision is insufficient.
3. Preserve preparation for recently culled static items within a bounded per-view ownership structure. Do not retain stale scene generations or resource publications; explicit removal/invalidation and shutdown release the cache. The public collection boundary records and compares scene identity, scene revision and resource publication revision before consuming a previous snapshot, including direct callers outside RenderSession. A mismatch leaves the old snapshot intact and starts a new collection without its retained entries. Continue querying each shadow view independently and retain existing ordering/culling/failure semantics. Improve collection lookup only where new phase scopes confirm meaningful cost.
4. Keep immutable submitted packets and GPU blocks. Any refreshed pass receives new packet ownership; old graphs and submitted frames continue observing their own values. Native D3D12 validation and fence retirement remain in their existing paths.
5. Use existing `HYP_PERF` category gates and Viewer CSV for stable diagnostic counters. Measure each implementation stage, retaining failed or regressing results. Only pursue maintenance/culling changes when the updated trace demonstrates that they remain material to the target.
6. The large-motion trace still spends 5.09 ms/frame in planning after view-level reuse, including 1.70 ms in instance data, 1.16 ms in input validation and 0.99 ms in candidate validation. Publish an item-level local proof as well as the ordered view proof. Its validity requires stable scene/resource publication and the same immutable preparation/local values; mixed and Draw dependencies remain excluded. Retain it with culled items, and use it to reuse prepared inputs, candidate structure and packed records when only visible membership changes. Packed records keep at most eight weak view proofs; other views use full value comparison. Keep full shared-value compatibility, target checks, ordering barriers and source accounting.
7. Canonicalize equivalent contracts across frames through a bounded weak history. Use complete-key hash indices for caches that do not require ordered queries, retaining explicit LRU and complete equality. Default provider outputs share only their validated single-semantic copy, preserving release of unrelated source resources. A measured adjacent-world frustum reuse experiment had no hits and was removed; the culling algorithm remains unchanged.

Alternatives considered: more single-draw micro-optimizations do not address the measured per-source CPU work; increasing cache budgets does not eliminate repeated validation; changing to persistent GPU object slots is a larger ABI change and current instance upload volume is already small; view-level all-or-nothing caching is the source of the current cliff.

## Risks / Trade-offs

- Incorrectly classifying a semantic as shared could freeze object-dependent values. Derive eligibility from full provider dependencies and active default/instance bindings, and test mixed, missing/default, override and resource transitions.
- A reused structure could hide changed source coverage or custom strategy decisions. Require ordered identities and explicit local validity; keep general fallbacks and negative tests.
- Retaining culled items could extend resource lifetimes. Bound retained entries, tie validity to scene/resource publications, and test removal, old snapshots and cache limits.
- Debug wall time includes scheduling and GUI costs. Run GPU workloads serially, keep runtime-off A/B distinct from traces, report distributions and operation counts, and verify rendered GUI readback separately.

## Migration Plan

Add contracts and diagnostics, implement and measure in stages, run targeted semantic/ownership regressions and scene equivalence, then build/test Debug and Release. No assets or serialized data migrate. Reverting the change restores the prior CPU path. This request does not require a commit or OpenSpec archive.

## Open Questions

The final large-motion trace still records 2.74 ms of planning, 1.73 ms of snapshot preparation and 2.96 ms of RHI pass preparation per frame, with parent/child overlap. The view proof avoids admission only when membership remains stable; changing membership still requires grouping and current receipts. Frozen Debug GUI measurements reach 13.29 ms mean / 15.90 ms P95 for small motion but 18.23 / 20.85 ms for large motion. The fixed 60 FPS target is therefore only met at P95 in the small-motion condition, and the static/moving gap remains substantial. The delivery report preserves these shortfalls; this change does not establish the user's desired small FPS variation.
