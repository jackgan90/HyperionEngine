## 1. Profiling foundation

- [x] 1.1 Implement engine-owned static scope/function/category/value/plot APIs, full session context and compile-out/runtime-off paths.
- [x] 1.2 Migrate legacy scopes/tests, remove mandatory aggregate timing and add controller-managed sampling with explicit availability.
- [x] 1.3 Verify location/nesting, category side effects, exceptions, mid-scope toggles and reconnects using a real Tracy capture.

## 2. Runtime controls and permanent hotspots

- [x] 2.1 Add Viewer CLI capture windows and GUI compiled/enabled/connected/category controls without persisted config changes.
- [x] 2.2 Instrument selected frame, scene/material, RHI validation/recording, upload, submission and wait phases; gate fine detail separately.
- [x] 2.3 Publish material evaluation-path counts and existing provider/constant/GPU cache counters per frame.
- [x] 2.4 Add an optimized profiling build with symbols and preserve default offline builds.

## 3. Capture workflow

- [x] 3.1 Build compatible headless capture/export tools from the pinned dependency through a repository helper.
- [x] 3.2 Automate bounded scene capture, warmup, export and metadata/log output with failure/child-process cleanup.
- [x] 3.3 Validate a real moving-camera trace and exported phases/counters while preserving draw workload.

## 4. Tasks and GPU profiling

- [x] 4.1 Add task IDs, queue latency, execution/wait scopes and stable thread names.
- [x] 4.2 Handle scope suspension/resumption across oneTBB waits and verify nested single-worker/error paths.
- [x] 4.3 Add bounded D3D12 pass timestamp recording, calibration and Tracy GPU event presentation.
- [x] 4.4 Collect timestamps using existing fence progress; validate normal frames, cancelled recordings and failed-Present recovery.

## 5. Validation and documentation

- [x] 5.1 Measure optimized scope microbenchmark and fixed-scene compiled-out/runtime-off/basic/detail-GPU capture overhead.
- [x] 5.2 Run appropriate Debug/Release regression suites, enabled profiling integration, default offline acceptance and style/naming/boundary checks.
- [x] 5.3 Document API usage, controls, capture commands, timing semantics, limitations and measured evidence; complete OpenSpec validation and task status.
