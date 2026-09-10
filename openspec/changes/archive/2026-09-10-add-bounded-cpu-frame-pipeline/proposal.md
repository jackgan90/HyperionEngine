## Why

Viewer serializes each Main tick behind Render and each Render frame behind the RHI coordinator. Bounded CPU frame overlap will allow the three stages to progress independently without unbounded latency or unsafe shared frame state.

## What Changes

- Add startup configuration for nonnegative Main-to-Render and Render-to-RHI lead limits, defaulting to one frame at each boundary.
- Add Renderer-owned ordered frame admission, completion, error observation and drain contracts; zero lead waits immediately after dispatch.
- Preserve at least one RHI thread, same-frame parallel recording joined by RHI 0, ordered submission and existing GPU fence retirement.
- Transfer frame inputs by owned values and immutable snapshots; separate mutable per-frame preparation/statistics and publish results only after completion.
- Integrate skipped ticks, resize, screenshots, capture, benchmark correlation and shutdown with bounded frame ownership.
- Add deterministic scheduling and native rendering regression coverage and documentation.

## Capabilities

### New Capabilities
- `bounded-cpu-frame-pipeline`: Ordered CPU stage overlap with independent lead limits, owned frame data, failures and lifecycle integration.

### Modified Capabilities

None. Existing rendering, GPU retirement and same-frame RHI recording contracts remain applicable.

## Impact

Renderer public frame execution and preparation interfaces, Viewer lifecycle/diagnostics, reflected configuration and CLI, tests and docs. No additional dependency, GPU scheduling change, independent-RHI removal, concurrent multi-frame RHI recording, dedicated data queue optimization, archive or Git commit.
