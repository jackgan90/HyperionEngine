## Why
Rendering needs explicit CPU ownership and safe task completion before platform and GPU code are added.

## What Changes
- Introduce a oneTBB-backed worker executor and engine-owned Main, Render and indexed RHI queues.
- Add dependency-aware dispatch, handles, waits, exception propagation and orderly shutdown.
- Expose execution counters for later diagnostics.

## Capabilities
### New Capabilities
- `task-execution`: CPU tasks, dedicated execution domains and completion semantics.
### Modified Capabilities
None.

## Impact
Adds the task wrapper, oneTBB runtime deployment and concurrency integration tests; public APIs remain vendor-free.
