## 1. Measurement foundation

- [x] 1.1 Preserve and hash the original Debug/Release/Profile runtime binaries and baseline evidence.
- [x] 1.2 Add reusable full-engine CPU workload generation, repeated runs and strict readiness/draw/submission validation.
- [x] 1.3 Add prepared-packet native recording measurements and stable missing profiling scopes/operation counters.

## 2. Material and batch refresh

- [x] 2.1 Implement prepared dependency/index metadata and bounded immutable shared refresh reuse for compatible parameters.
- [x] 2.2 Verify arbitrary/mixed scopes, overrides, defaults, resource transitions and retained old results with focused tests.
- [x] 2.3 Separate built-in batch grouping/shared-value refresh from instance payload invalidation, retaining custom strategy fallback.
- [x] 2.4 Move global batch cache retirement to a family boundary and verify membership/order/coverage and bounded retention.
- [x] 2.5 Build and measure material/batching improvements against preserved binaries.

## 3. Native recording

- [x] 3.1 Pool D3D12 command lists under completed frame-slot/context ownership and reset fresh list state.
- [x] 3.2 Cache remaining graphics state and remove avoidable binding validation scratch work without dropping validation.
- [x] 3.3 Test alternating state, retained work, frame reuse, cancellation and failed-Present paths; measure native recording slope.

## 4. Data ownership and scheduling

- [x] 4.1 Reduce render-graph/recording packet copies with explicit consuming or immutable shared ownership.
- [x] 4.2 Remove redundant collection state copies and consolidate compatible RHI statistics/task work.
- [x] 4.3 Verify graph lifetime/plugin behavior and measure remaining view/family scheduling costs.

## 5. Acceptance and delivery

- [x] 5.1 Complete Debug/Release/Profile builds, style/naming/boundary checks and affected correctness/lifetime tests.
- [x] 5.2 Run interleaved repeated 0/1/100/300/600/1200 and static/moving/CSM benchmarks with exact coverage and unchanged validation.
- [x] 5.3 Verify representative images, visible UI, rapid camera motion and sustained-run P95/P99/resource stability.
- [x] 5.4 Publish commands, measured stage/full-frame gains, remaining limitations and final OpenSpec validation.

## 6. Independent audit corrections

- [x] 6.1 Retire obsolete batch-plan parameter owners on expired emitted owners and scene invalidation, preserving externally retained frames.
- [x] 6.2 Protect constant pages for shared and inline owned commands, validate every command shell, and cover recorded/in-flight/final-release behavior.
- [x] 6.3 Complete targeted multi-configuration validation, independent re-review and performance-impact evidence for the audit fixes.
