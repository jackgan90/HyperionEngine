## Why

GPU benchmark attribution currently depends on display-name prefixes and parses cascade indices from pass names. Renaming a pass can silently change CSV values even when the rendered work is unchanged. The sixth maintainability item separates this statistical identity from display text.

## What Changes

- Give Renderer passes an explicit timing category and category-local instance index; assign these where built-in passes are declared.
- Carry an opaque, owned value tag through RHI commands and completed GPU timings without introducing Renderer concepts into RHI or native backends.
- Aggregate benchmark columns from typed metadata, preserving existing columns, category membership, totals, submission matching and timing lifecycle.
- Verify renamed and misleading names, split graphics batches, compute passes, delayed GPU completion and the existing CSV oracle.
- Custom passes that previously relied on reserved display prefixes must explicitly opt into a Renderer timing category; unclassified passes continue to contribute to the total.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `renderer-cpu-benchmarks`: GPU category attribution must use typed Renderer metadata rather than pass names while retaining the CSV contract.
- `rhi-backend-abstraction`: pass timing transports opaque caller metadata through recorded commands and fence-complete results.

## Impact

Renderer pass declarations, graph compilation, benchmark aggregation and D3D12 timing collection; existing graph, renderer and RHI regression tests and diagnostic documentation. No new dependencies, asset/schema/operation IDs, GUI controls or automation operations. This is a structural change, not a performance optimization. Archive and Git commit remain separate follow-up actions.
