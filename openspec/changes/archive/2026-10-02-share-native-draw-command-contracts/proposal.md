## Why

The D3D12 ordinary recorder and cached draw-plan builder independently expand geometry views, dynamic state and indexed-draw arguments. Cached commands then encode these values in unnamed integer arrays and generic pointers. M08 consolidates that interpretation and makes payloads reviewable while retaining the measured submission optimization and existing lifetime safeguards.

## What Changes

- Establish a Release performance and command-storage baseline before changing parameter expansion or cached representation.
- Share backend-private native geometry/dynamic/draw value construction between ordinary recording and plan creation (M08A).
- Replace positional cached command payloads and bind statistics with named typed values without adding per-command ownership or virtual dispatch (M08B).
- Verify fixed parameter expectations, native output, first recording/build/reuse/invalidation, constant-page retention and before/after performance.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `rhi-backend-abstraction`: Equivalent ordinary and cached native draw submission with retained validation, ownership and measured reuse.

## Impact

D3D12 private recorder and plan implementation, same-module native test support, existing RHI tests/submission benchmark, and focused RHI documentation. Public RHI commands, formats, statistics wire schemas and plugin composition remain stable. No new draw feature, backend, generic command interpreter or lifetime model is introduced.
