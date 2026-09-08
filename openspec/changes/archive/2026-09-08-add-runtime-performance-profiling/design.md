## Context

HEAD 06b678b contains four production `FProfileScope` sites and a frame marker. The wrapper captures its own source location, discards Tracy's on-demand connection ID, and always performs two clock reads and two global atomic additions. The material performance investigation used temporary timers; its existing permanent counters already cover providers, constant packing/uploads and GPU cache reuse. Worker waits use oneTBB resumable tasks, and D3D12 records multiple command lists concurrently before submitting them through one direct queue.

## Goals / Non-Goals

**Goals:** one-statement instrumentation with correct locations and nesting; compile-out and cheap runtime disable; selective permanent coverage; correlated workload counters; repeatable captures with symbols; safe task suspension and GPU timestamp lifetime; measured overhead.

**Non-Goals:** exhaustive instrumentation, a new trace format/UI, automatic compiler instrumentation of every function, a material optimization redesign, automatic elevation or system configuration changes, or stopping/restarting the entire Tracy client on every UI toggle. Tagged allocator accounting and required application clocks are separate from profiling. Full-process allocation interception is not introduced.

## Decisions

### 1. Core owns a small profiling API

Use `Core/Public/Hyperion/Core/Profiling.h` plus focused private Tracy adapters. `HYP_PERF_SCOPE(Name)` stringizes an identifier; named/category variants and `HYP_PERF_FUNCTION()` capture static caller metadata. Unique macro-local identifiers allow more than one scope per block. Plot/value expressions are evaluated only when their category is enabled. The compilation definition is engine-owned and propagated to clients through Core; Tracy headers/definitions remain private.

Each static site owns fixed backend storage, initialized once only after activation and connection. Store the entire native zone context behind the adapter, including connection generation; no dynamic source-location allocation or global name lookup on repeated events. Preserve begin/end pairing when runtime masks change. Remove legacy unconditional scope totals and migrate the four production sites and Core test. Frame intervals and allocator statistics remain available separately.

### 2. Build and runtime controls have explicit costs

Keep `HYP_ENABLE_TRACY=OFF` as the default. Compiled-out macros discard arguments and all profiling calls. Runtime state is an atomic category mask; disabled scope construction/destruction remains inline and avoids clocks, registration, allocations and global RMW operations. Main/Render/RHI/Material/Tasks/Assets and a detail category permit selective collection; GPU and sampling require explicit activation. Coarse CPU categories form the default capture; fine per-item scopes belong to Detail.

Tracy remains on-demand. Its system sampling uses manual start/stop, serialized through the controller and disabled when runtime profiling is disabled. Report unavailable sampling distinctly and do not elevate automatically. Existing tagged allocation/free Tracy hooks continue as one coherent on-demand stream rather than gating alloc and free independently. Their accounting coverage is documented. Local-only/no-broadcast defaults minimize exposure for profiling builds; remote users can explicitly override dependency options. Compiled-in idle builds can retain Tracy service threads/listener, which is included in real-process overhead measurements.

Viewer adds explicit profiling CLI options, bounded warmup/capture-window controls and runtime GUI toggles. Settings are session-local, not silently serialized into experiment files. Sampling is a separate switch. Profiling builds retain Release optimization and add MSVC compile/link symbols; no Debug-only performance claims.

### 3. Permanent coverage follows meaningful phases

Coarse scopes cover input/GUI/update, Render material/scene preparation, RHI draw preparation, validation/recording, submission, fence waits and Present. Detail scopes split full material evaluation versus refresh, constant-cache lookup/packing/upload and binding work. Avoid instrumentation inside parameter/index element loops. Scene pass event identity stays static; frame/view/draw IDs are values rather than ever-growing site names.

Publish per-frame deltas of provider, constant and GPU cache counters, plus evaluation reuse/refresh/full counts and draw count. Snapshot on the owning execution domain and publish outside hot loops. Plots supplement timelines: inclusive wall time, running CPU time and GPU duration must not be conflated or summed across threads as frame time.

### 4. Reproducible capture tools reuse the locked Tracy source

Build the headless capture and CSV export utilities from `out/deps/tracy` into a dedicated output directory; do not assume an arbitrary installed profiler is compatible. A Python helper orchestrates the existing build helpers, collector, Viewer workload and export, propagates errors, bounds waits and cleans up only its child processes. Save `.tracy`, frame CSV, zone CSV, console logs and a metadata JSON containing revision, build, switches, scene, warmup and sample counts. Preserve current scene-readiness/warmup validation. Interactive viewing uses a matching Tracy GUI; headless capture requires no GUI build.

### 5. Task profiling records execution segments and association

Dedicated queues and Worker execution get stable names and task IDs, with queue latency measured only when task profiling is enabled. A general RAII scope must not end on a different OS thread from its start. Implement an engine suspension boundary that closes active CPU zones before a resumable wait and reopens their sites after resumption, retaining task identity. Suspended time is excluded from Worker execution segments; dedicated blocking waits remain explicit wall-time scopes. This avoids requiring every importer or future task author to understand vendor fiber hooks. Validate nested single-worker waits, exception paths and runtime/connection changes while suspended. This segment approach is preferred to a permanent per-task fiber-name registry with unbounded names.

### 6. GPU profiling is automatic at the native pass boundary

D3D12 timestamps enclose recorded pass commands and resolve to readback resources retained through existing submission fences. Fixed/bounded frame/recording slots prevent unbounded query growth; no profiling-only fence wait, queue flush or spin is introduced. Collect only completed submissions, including shutdown/failed-Present drain. Lists cancelled before submission publish no GPU span and release/reuse their slots safely. Begin/end queries must be recorded before closing the command list. Use engine-owned GPU event data through the Core adapter so native objects remain private to D3D12. CPU recording and GPU execution retain distinct tracks and timestamps. Unsupported profiling or resource exhaustion skips telemetry without changing rendering behavior.

The stock Tracy D3D12 helper allocates query IDs while recording; abandoned lists can leave unresolved IDs and pool-pressure stalls. Prefer bounded backend-owned queries tied to Hyperion's existing submission lifecycle, with Tracy used for presentation/serialization. Calibration and session generation must prevent timestamps from an earlier connection entering a new capture. Query resource creation and collection are tested separately from recording.

## Risks / Trade-offs

- Closed scopes during worker suspension appear as multiple execution segments → document this semantic, preserve task IDs, and validate exported durations rather than pretending they are continuous CPU work.
- Tracy is not cost-free when compiled in but idle → compare compiled-out and runtime-off executable runs, and retain default OFF.
- Dynamic names/callstacks and dense markers can dominate tiny functions → static sites, category/detail gates, no per-element loops, capture overhead evidence.
- Symbols or kernel permissions may be missing → report the limitation; ordinary scope tracing remains usable.
- GPU queries can corrupt rendering if reused early → bounded storage, fence retirement, cancelled-list tests and failure recovery tests.
- Vendor contract drift → use the locked dependency for tools and adapters; test real traces/reconnections rather than only counters.

## Migration Plan

Implement the Core API and trace correctness first, then runtime controls and hotspots, then capture tooling, then task and GPU integration. Check each task as evidence completes. Run default Debug/Release regression suites and a dedicated profiling build, format/naming/boundary checks, real trace assertions, and a controlled overhead comparison. Document commands and measured limits in `docs/Profiling.md` and change-local implementation evidence. No automatic commit or archive is required by this implementation request.

## Open Questions

None blocking implementation. Actual per-scope and frame overhead, sampling availability and GPU timestamp precision are validation results, not assumed guarantees.
