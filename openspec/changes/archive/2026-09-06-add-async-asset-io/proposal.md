## Why

Asset libraries currently open files directly and there is no IO executor. Engine-owned byte IO is required before asynchronous model loading can control storage, ownership and shutdown.

## What Changes

- Add one IO execution domain to FTaskSystem without changing existing domain names or queue ownership.
- Add Runtime/IO with local and injectable storage, bounded owned byte reads, atomic writes, cancellation and statistics.
- Separate physical IO from worker decoding; retain explicit synchronous bootstrap/test compatibility.

## Capabilities

### New Capabilities
- `async-asset-io`: Asset libraries currently open files directly and there is no IO executor. Engine-owned byte IO is required before asynchronous model loading can control storage, ownership and shutdown.

### Modified Capabilities
None. Existing configuration, triangle and task behavior is preserved.

## Impact

Tasks, new IO module, CMake, task and IO tests. No new vendor dependency.
