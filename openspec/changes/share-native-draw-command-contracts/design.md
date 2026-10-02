## Context

M08 is confirmed in `D3D12Draws.cpp::BindGeometry/BindDrawState/RecordDraws` and `D3D12DrawPlan.cpp::FPlanBuilder/ExecuteCommand`. Both paths expand the same R32 indexed geometry and draw fields. The plan stores an operation tag plus Slot, Value, four integer Arguments and void Pointer; bind counts use seven positional integers. The current code works for supported inputs; independent expansion is a maintenance risk, not evidence of an existing pixel defect.

`PrepareNativeDraws` first validates shell storage. A miss registers constant-page owners before validating draws, updates weak cache identity and returns no plan so ordinary recording executes. The second matching shared stream builds the plan; later matches reuse it. Recorded/submitted commands retain the actual immutable stream through fence completion. The plan's borrowed native pointers depend on those owners; the cache must never become a resource root.

## Goals / Non-Goals

**Goals:** One native interpretation of geometry/dynamic/indexed arguments; clear cached payloads and statistics; proven output/validation/lifetime equivalence; measured submission and storage cost.

**Non-Goals:** New index formats, vertex streams, indirect drawing, public native types, generic interpreter framework, new GPU backends, plugin changes, per-frame full revalidation, per-command shared_ptr ownership, or changing the cache admission lifecycle.

## Decisions

### Baseline is an implementation gate

Before M08A/B production refactoring, extend only benchmark/test fixtures and minimal read-only private plan introspection needed to report existing behavior. A baseline preparatory diff must be reviewed for absence of parameter, cache or execution changes. Preserve the existing `submission_benchmark Output.csv Warmup Samples [--owned]` interface; an additive state-switch workload selector is allowed. Existing draw counts remain 0, 1, 100, 300, 600 and 1200. The additional workload must alternate real pipelines/root layouts/resource bindings, geometry and dynamic values, not only a synthetic counter. Keep fixture creation outside the timed region.

The orchestrator builds Release and serially runs at least three trials of ordinary and owned paths for homogeneous and switching workloads with identical warmup/sample counts, resolution, device and settings. Start with 60 warmup/180 measured frames per draw count, adjusting both before/after only for a documented measurement issue. Save raw CSV, command output, binary/source hashes and settings. Report median and p95 per count/workload/path plus between-trial variation. Snapshot the baseline executable and required runtime DLLs so paired remeasurement remains possible after implementation.

Record `sizeof` of a native command, command count, vector capacity bytes, and actual cold/build/reuse/invalidation behavior through backend-private read-only metrics and a same-module native test TU. A pure plan-description helper plus direct calls to real preparation functions are sufficient; add no persistent hot-path log or public RHI/wire statistic just for this change. Metrics must come from real generated plans, not estimates from draw count. Obtain the device state through valid backend-owned resources in that private test seam, never pointer reinterpretation through public interfaces.

### M08A: share value construction, retain recording state machines

Add small private native values/helpers for vertex/index views, scissor/dynamic input and indexed-draw arguments. Helpers take already-resolved native resources or validated values and perform only deterministic construction. Both ordinary recorder and plan builder consume them, including index format comparison even while R32 remains the sole supported format. Preserve signed VertexOffset, nonzero FirstIndex, IndexCount, InstanceCount, buffer extent and vertex stride exactly; StartInstanceLocation stays zero.

Do not combine resource resolution, ownership validation or binding caches into the helper. Ordinary recording retains its local state suppression and direct native calls. Plan creation retains its own state suppression and uses the same interpreted values. Avoid allocating an intermediate command stream for every ordinary recording. This keeps the cold path cheap and reduces the duplicated contract without forcing identical execution machinery.

### M08B: typed trivial payloads and named statistics

Use explicit operation tags and a trivially copyable union (or an equally small measured representation) whose active payload has named fields. Native pointer types remain specific: pipeline state, root signature and descriptor heaps; buffer views and GPU descriptor/address fields retain native types. Blend/scissor/draw payloads use actual typed values, eliminating integer bit-casts used solely for storage and void-pointer recovery. Use constructors/factories that initialize the selected union member correctly and keep discriminant/payload construction together. `static_assert` trivial copyability and relevant layout/size properties.

Use a named private bind-statistics struct for Root, Heap, Constant, Table, Pipeline, Geometry and Dynamic counts. Accumulation into existing device statistics and profiling names remains unchanged. A generic variant, virtual command hierarchy or per-command owning allocation is not selected; any alternative needs explicit size and measured execution justification before acceptance.

### Preserve cache and resource ownership

Keep PrepareNativeDraws admission, all target/access/sample identities, shell checks and weak-owner semantics intact. Register every nested constant-page owner before validation on a miss; no new allocation failure may publish a successful reuse proof early. A matching plan uses borrowed native pointers while the recorded FPassCommands owns the stream and resources until the existing fence retirement. Expired streams must release constant pages after completed work even if the cache still exists. Do not add strong handles to cached payloads or skip target/access checks on hits.

Tests cover first miss/ordinary, second build, later pointer reuse, a new shared stream with changed arguments, and retained old immutable commands. Reuse is an explicit observed lifecycle, not inferred only from faster timing. Keep the existing nested constant-page reset, malformed shell, incompatible target/geometry, foreign resource and dead-stream release tests.

### Independent semantic and performance acceptance

Use fixed expectations independent of production helpers for all geometry/draw/dynamic fields, including legal negative VertexOffset, FirstIndex, instance count, alternate buffer/stride, signed scissor values and floating blend constants. Execute ordinary, cold owned and warm owned recordings with native D3D12 readback. Assert fixed pixel expectations where feasible in addition to comparing paths, so equivalent mistakes cannot pass. Exercise pipeline/root/table/constant/geometry/dynamic changes, and verify named bind totals against observed expected sequences.

After M08A and M08B, build relevant targets and run the same workload protocol. Final comparison includes 0/1 overhead and heavier counts; investigate median regressions above 10% that also exceed three times observed between-trial noise, and any consistent p95 or storage growth before acceptance. This is a review trigger, not permission to silently accept a slower result below a threshold. For noisy results, repeat paired saved-baseline/current trials under quiet conditions. If a repeatable regression remains, revise the implementation within scope; unresolved evidence must stay open. Report measured values and uncertainty, not a general speedup claim from one run.

## Risks / Trade-offs

- Tagged union lifetime or uninitialized member → trivial payloads, explicit construction and fixed native-field tests; avoid inspecting inactive members.
- A larger payload slows cache traversal → compare sizeof/count/capacity and measured homogeneous/switching cases; no unnecessary ownership per command.
- Shared helpers accidentally perform repeated resolution or allocations → keep inputs pre-resolved and verify cold-path measurement.
- Borrowed pointers outlive streams → preserve existing weak cache/strong recorded owner split and nested-page/fence regressions.
- Concurrent builds/benchmarks distort timing → orchestrator serializes builds, GPU tests and timed trials, recording the actual environment.

## Migration Plan

Review baseline-only fixtures and capture Release evidence; implement M08A and verify fixed arguments/output; implement M08B and verify lifecycle/size/output; repeat Release measurements, then final Debug/Release RHI/device/compute/instance tests plus style/naming/boundaries and independent frozen review. The change remains one proposal with separate A/B task checkpoints. No persistence or wire migration is needed; rollback restores the prior private recorders without asset changes.

## Open Questions

Exact trivial payload packing is chosen after the measured existing command size and real plan storage are known. This is a bounded implementation choice; baseline completion is mandatory before production refactoring.
