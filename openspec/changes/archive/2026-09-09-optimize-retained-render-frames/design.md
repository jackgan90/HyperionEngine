## Context

Round one's measured 600 ordinary draws still cost 20.67 ms static / 31.98 ms moving in Debug and 2.57 / 4.48 ms in Release. Profile traces identify repeated Collect/visibility/material/packet loops, approximately 600 immutable material commits on a moving camera, per-view resource maintenance dispatch, and six or ten task waits per frame. Inclusive TaskWait durations overlap Main and Render and are not additive overhead.

The existing graph has Render collect/evaluate all views, a synchronous RHI preparation task, another BeginFrame round trip, one task per command list, recording joins, then another EndFrame round trip. Dedicated executors cannot synchronously wait on their own pending work. GPU frame fences and failure recovery remain mandatory.

## Goals / Non-Goals

**Goals:**
- Stable built-in scene/view work becomes retained data; unchanged warmed frames avoid collection, parameter resolution, batch planning, and packet construction.
- Scope/view changes invalidate dependent preparation with generic material semantics and conservative extension fallback.
- Main publishes changes and waits for Render once; Render passes owned work to one RHI frame coordinator and waits once in the ordinary Viewer path. Indexed recorders have explicit bounded joins.
- Preserve draws, order, pixels, diagnostics, validation, resource retirement, and useful operation counters. Measure first-round versus second-round binaries.

**Non-Goals:**
- No frame dropping, reduced CSM quality/update rate, delayed scene changes, disabled validation, or in-place mutation of submitted constants.
- No general render-thread/RHI thread merger, unrelated scheduler replacement, or GPU-driven rendering implementation.

## Decisions

1. **Retained preparation with explicit invalidation.** Track Render scene mutation and resource publication revisions. Cache only collection contracts proven stable (built-in static primitives; custom types default to conservative collection). View visibility/sort keys include the actual matrices and usage, not only caller revision numbers. Retain immutable material/packet data while checking engine scope dependencies. Frame/Pass/Draw dependencies and arbitrary strategies continue to evaluate as required. Caches remain bounded and release old scene/resource owners on invalidation/close.
2. **Incremental Main and maintenance work.** Use logical change records plus tracked editable material revisions, so scenes without mutable material users have an empty-work path. Readiness/status caching must invalidate on scene publication and resource completion/failure. Resource creation, release, pending upload/fence retirement, and failed completion queries schedule maintenance; ready view preparation alone does not.
3. **Owned deferred graph preparation.** Graph entries can hold owned RHI preparation functions that expand to passes at their ordered position. Existing synchronous preparation APIs remain available. Render collects/evaluates scene data; the frame coordinator materializes packets/GUI resources, compiles/validates the graph, begins the frame, records and submits. Returned statistics are published back after the single boundary wait.
4. **One coordinator per frame.** Run frame execution on RHI 0, execute its recording partition inline, dispatch at most one recording batch per other used RHI executor, then join peers before ordered submission. No same-executor dispatch/wait. All admitted peers are joined before cancellation on any failure. Capture and GUI preparation enter the same explicit boundary where applicable.
5. **Immutable draw reuse reaches native recording.** Retained packets must not be deep-copied through graph/recording layers. Native validation/state planning may reuse proofs only for owned immutable inputs with complete target/resource compatibility; mutable borrowed requests remain fully validated. Actual draw calls remain recorded each frame. Measure preparation, binding/state work, API recording, queue delays and GPU/present waits separately.
6. **Share refreshed engine input ownership.** Material evaluations retain one immutable engine-input block per dependency set in a view. Resolved Object/Material/Draw scope data stays separate; numeric camera refresh replaces a shared engine block instead of copying all scope keys per item. Mixed dependencies, absent-provider defaults, and Frame/Pass/Draw changes keep their established evaluation behavior. No block links to an older evaluation.
7. **Reuse stable shadow setup and native plans.** CSM projection/caster setup uses actual camera/light/settings values plus stable scene/resource revisions. Dynamic query clients without a revision still query normally. Native plans are cached weakly per recording context after a successful validation; a later immutable-stream/target match compiles and replays only necessary binding and draw calls. Constant pages register weak ownership credentials for their immutable stream or inline command owner before validation. Registration, publication checks and reset share a page mutex; reset requires both a sole buffer handle and no live command credentials. Command-shell draw-storage invariants are checked before every cache lookup. Borrowed recording still snapshots and validates independently.
8. **GPU timing capture membership follows submission.** A capture epoch is attached to each admitted native submission. Completion can occur later, but samples from before capture start or from a previous capture cannot enter the current capture. This fixes a timing-dependent boundary exposed by removing idle maintenance without adding waits.

## Risks / Trade-offs

- Cached state hides resource readiness or material changes → publication revisions and complete input keys; negative invalidation tests and reference images.
- Custom collection/providers/strategies have broader dependencies → opt-in stable collection contract and conservative fallback; never infer stability from addresses alone.
- Retained ownership delays reclamation → bounded current entries, weak resource identities where possible, clear on removal/close, no-frame retirement tests.
- Deferred expansion changes graph dependency indices → explicit ordered entries and remapping to expanded ranges; reject invalid/cyclic dependencies before frame acquisition.
- Coordinator waits on its own queue or misses peer errors → inline its partition, join other executor batches, fault/retry tests for preparation, recording, submission, and cancellation.
- Profiler/task sums misrepresent latency → label inclusive values and examine actual critical-path gaps; compare unprofiled binaries for end-to-end performance.

## Migration Plan

Preserve round-one sources/binaries/evidence, implement and test maintenance/scheduling and retained data in separately measured stages, then run Debug/Release/Profile correctness and matched static/moving sweeps. Existing public immediate APIs remain valid. Record remaining cost and target gaps without changing benchmark settings.

## Open Questions

The exact remaining hot path after retention is empirical. Additional material/native shortcuts will be accepted only with complete invalidation proof and measured benefit; unsuccessful experiments will be removed or documented before delivery.
