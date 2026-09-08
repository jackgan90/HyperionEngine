# runtime-performance-profiling Specification

## Purpose
Provide low-overhead engine-owned CPU and GPU profiling, selective runtime controls, targeted permanent instrumentation and reproducible captures for locating performance bottlenecks.
## Requirements
### Requirement: Engine-owned scoped instrumentation
The engine SHALL provide one-statement CPU scope/function macros and optional categories, values and plots without public vendor includes. Scopes SHALL retain caller file, function and line and end on lexical exit, including exceptions. Repeated sites SHALL use stable static identity.

#### Scenario: Distinct nested callers
- **WHEN** two functions and a nested block execute instrumented work during a capture
- **THEN** the trace distinguishes their source locations and records balanced nested intervals

#### Scenario: Runtime gate changes inside a scope
- **WHEN** capture categories are disabled before an active scope exits
- **THEN** its original begin event still receives a matching end in the same capture session

#### Scenario: Reconnection
- **WHEN** the collector disconnects and reconnects while work continues
- **THEN** new scopes have balanced events and old-session events are not attributed to the new session

### Requirement: Low-overhead disable and selective capture
`HYP_ENABLE_TRACY` SHALL default to OFF. Compiled-out instrumentation SHALL discard arguments. Runtime-disabled scopes SHALL avoid clock reads, site registration, allocations and global atomic additions. Basic CPU collection, detailed hotspots, GPU collection and system sampling SHALL be selectable; profiling builds SHALL retain optimized code and generate matching symbols.

#### Scenario: Side-effecting plot argument
- **WHEN** a disabled plot macro is passed an expression with a side effect
- **THEN** that expression is not evaluated in compiled-out or category-disabled operation

#### Scenario: Sampling and offline baseline
- **WHEN** runtime profiling is disabled
- **THEN** system sampling is stopped or remains unstarted, and documentation distinguishes idle compiled-in services from the compiled-out no-listener baseline

### Requirement: Viewer profiling controls
Viewer SHALL expose session-local CLI and GUI controls for profiling with visible compiled/active/connected state. Unsupported profiling requests SHALL fail clearly. Capture windows SHALL support warmup and a bounded number of frames without changing experiment persistence or workload behavior.

#### Scenario: Toggle during interaction
- **WHEN** the user changes the profiling controls
- **THEN** the displayed state and subsequent eligible events reflect that change without restarting the rendering application

### Requirement: Targeted permanent coverage and counters
Owned code SHALL contain permanent scopes at selected likely hotspots across frame work, scene/material preparation, RHI validation/recording and waits. Instrumentation SHALL include draw validation before recording and distinguish Present/fence waits from CPU preparation. Material evaluation paths and existing provider/constant/resource cache metrics SHALL be published as frame-correlated values or plots.

#### Scenario: Moving scene capture
- **WHEN** a loaded scene is rendered with benchmark camera motion
- **THEN** the trace contains identifiable material, draw preparation, validation, recording and waiting stages and material workload counters alongside frame markers

### Requirement: Reproducible performance capture
Repository tooling SHALL build compatible headless Tracy tools from the locked source and produce a trace, zone statistics, frame samples and run metadata for a bounded workload. Collector or application failure SHALL be reported and helper-owned child processes SHALL be cleaned up.

#### Scenario: Capture and export
- **WHEN** the documented helper completes a scene profiling run
- **THEN** the output contains readable `.tracy`, zone CSV, frame CSV, logs and metadata with revision and capture settings

### Requirement: Resumable task profiling
Task profiling SHALL expose execution-domain identity, task association and queue delay without recording invalid cross-thread CPU scopes. Worker suspension SHALL separate execution segments; task progress and error behavior SHALL remain unchanged.

#### Scenario: Single worker nested wait
- **WHEN** a profiled Worker task waits on a child using the only Worker
- **THEN** both complete, scope intervals remain valid across suspension/resumption, and suspended time is not counted as a running Worker segment

### Requirement: Asynchronous GPU pass profiling
The D3D12 backend SHALL support optional GPU pass timestamps and present them separately from CPU recording. Profiling query storage SHALL be bounded and retained until submitted work completes. Collection SHALL NOT introduce additional GPU synchronization waits. Cancelled unsubmitted recordings SHALL NOT publish GPU execution spans.

#### Scenario: Completed and cancelled frames
- **WHEN** a profiled frame is submitted and completed, followed by a recorded frame cancelled before submission
- **THEN** the submitted frame yields GPU timings, the cancelled frame yields none, and subsequent recording succeeds without unresolved query accumulation

#### Scenario: Failed Present
- **WHEN** Present fails after submitting profiled commands
- **THEN** profiling resources remain valid through the existing drain/recovery path and repeated cancellation remains safe

### Requirement: Profiling overhead evidence
Validation SHALL compare compiled-out, runtime-off and active capture costs using an optimized scope microbenchmark and a fixed real rendering workload. Results SHALL identify sampling/detail/GPU options and distinguish measurements from targets.

#### Scenario: Reported profiling cost
- **WHEN** implementation validation finishes
- **THEN** evidence includes scope overhead and frame mean/tail timings with workload/draw counts, and does not claim unmeasured zero-cost runtime behavior

### Requirement: Material optimization evidence
The engine SHALL expose low-overhead material work and cache statistics sufficient to distinguish shared reuse, actual evaluation, expensive constant lookup, packing/upload and bounded cache retention. Optimization verification SHALL record reproducible before/after frame timing and comparable workload/build metadata and SHALL include engine-level frequency and cache-pressure tests independent of the Viewer application.

#### Scenario: Before and after comparison
- **WHEN** material performance improvements are reported
- **THEN** Debug and optimized build results identify warmup, sample counts, draw counts, instrumentation and validation settings, and preserve underlying frame CSV and profiling evidence

#### Scenario: Generality validation
- **WHEN** a non-builtin material exercises different scopes and prolonged cache pressure through engine APIs
- **THEN** counters demonstrate the intended update frequency and bounded cache retention without application-specific fast paths

#### Scenario: Closed cache statistics
- **WHEN** resource service closure has destroyed the material constant cache and its pages
- **THEN** live page count and page capacity statistics are zero while cumulative work counters remain available
