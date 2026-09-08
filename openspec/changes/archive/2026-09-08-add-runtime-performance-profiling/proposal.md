## Why

The material rollout required temporary nested timers to locate expensive evaluation, binding and index-validation work. The existing Tracy wrapper loses caller locations and on-demand connection identity, and continues timing and globally counting scopes when Tracy is compiled out. Hyperion needs a correct, inexpensive, permanent profiling path that can be extended with one or two statements at suspected hotspots.

## What Changes

- Add engine-owned scope/function, category, plot and thread/task profiling interfaces with caller source locations, compile-time elimination and a cheap runtime gate.
- Replace the legacy always-on aggregate scope implementation and preserve complete Tracy session identity. **BREAKING**: remove the old string-only `FProfileScope` constructor and `ProfileStats` aggregate API after migrating all owned users; logging, clocks and tagged memory statistics retain their contracts.
- Add explicit sampling control, Viewer CLI/UI controls and optimized profiling builds with matching symbols. Keep ordinary builds offline by default.
- Permanently mark selected frame, material, RHI validation/recording, submission/wait and asset/shader hotspots; publish existing material/cache counters and evaluation-path counts. Avoid exhaustive or per-element instrumentation.
- Add a reproducible capture/export helper using tools built from the locked Tracy source, preserving workload, warmup, frame metrics and capture metadata.
- Integrate task execution/wait association safely across resumable workers, and bounded D3D12 pass timestamps with asynchronous fence-based collection and Tracy GPU presentation.
- Validate real traces, reconnects, runtime toggles, GPU cancellation/lifetime behavior and instrumentation overhead in addition to existing rendering tests.

## Capabilities

### New Capabilities

- `runtime-performance-profiling`: Engine-owned low-overhead CPU/GPU instrumentation, runtime controls, targeted coverage, task association and reproducible capture/export.

### Modified Capabilities

- `core-foundation`: Named CPU timing is conditional on profiling activation; disabled scopes have no mandatory local timing or aggregate updates.

## Impact

Core private Tracy adapters and public profiling API; Tasks scheduling and resumable waits; Renderer/material/cache phases; D3D12 recording, timestamps and retirement; Viewer CLI and DebugUI; CMake/build helpers; profiling tests and documentation. No new third-party dependency or replacement of the existing renderer, material caches, task scheduler or Tracy revision. GPU/OS sampling are explicit capabilities; CPU scope timing remains distinct from GPU execution and wall-clock waits. Full profiler teardown on every runtime toggle is outside scope: compiled-out builds provide the no-service baseline, while enabled builds retain a documented idle Tracy listener.
