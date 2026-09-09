## Why

At `8579250`, moving-camera SceneViewer remains faster with instance rendering, but large-motion Debug batch planning costs about 7.65 ms. The matching diagnostic trace attributes only 0.23 ms to grouping itself, versus 2.77 ms to instance data preparation and 1.76 ms to rebuilding cached plan inputs; the planner repeatedly reconstructs reflection/value/state information that is already known.

## What Changes

- Compile reusable batch/instance layout contracts once per complete program/pass contract instead of rebuilding layouts and parameter-index lists during each plan or chunk lookup.
- Retain stable, completely validated item preparation and instance-input identities across visibility changes. Shared numerical changes, local instance changes and structural compatibility changes receive distinct invalidation paths.
- Make view plans and chunk lookup consume lightweight prepared identities, eliminating duplicated full parameter snapshots and repeated member/value gathering across cache layers.
- Preserve complete effective compatibility, ordering barriers, custom-strategy behavior, exact source coverage, anonymous-item fallback, bounded retention and immutable GPU lifetimes.
- Add a benchmark that calls the real planner with controlled membership/order/value/resource changes, reusable detail counters, regression tests and frozen Debug/Release SceneViewer A/B evidence. Measure before adopting further structural changes.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `render-batching`: Require reusable prepared planning contracts and item proofs so visibility-only changes do not reconstruct stable reflection and instance-value metadata at every cache layer; preserve conservative extension behavior and bounded retirement.

## Impact

Renderer batch planning, plan/chunk/record caches, reusable profiling and benchmark tools, regression coverage and documentation. A broader internal refactor is authorized where supported by theory and measurements. Existing shader/native ABI, rendering work, Debug validation, source coverage and GPU publication rules remain intact. No new dependency or Git commit is required for this implementation round.
