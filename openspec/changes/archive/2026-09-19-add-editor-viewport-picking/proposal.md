## Why

The editor can select objects through the outliner and manipulate them with gizmos, but cannot select a model by clicking the viewport. Render owns the current visibility BVH; using it directly from Main would violate thread ownership and couple interaction to render scheduling.

## What Changes

- Extract reusable CPU bounds indexing into Math while retaining independent Render visibility and Main query state.
- Add Scene ray queries with incremental scene bounds maintenance and shared immutable mesh triangle acceleration prepared outside Main.
- Add reusable Renderer viewport ray construction consistent with scene camera projection, clipping and depth conventions.
- Integrate single-model click selection into Editor, giving gizmos and navigation priority and preserving empty selection.
- Validate geometric correctness, incremental invalidation, asynchronous lifetime, input routing and Render visibility compatibility.

## Capabilities

### New Capabilities
- `scene-ray-queries`: Main-owned accelerated geometric queries over the current CPU scene with stable handles and explicit availability.
- `editor-viewport-picking`: Viewport single selection using the active view, with click arbitration and selection lifetime rules.

### Modified Capabilities

None. Existing render visibility and document/history contracts remain applicable.

## Impact

Math gains CPU spatial primitives; Scene owns query data without Renderer/RHI dependencies. Renderer retains its visibility adapter and provides view-ray and asynchronous scene preparation integration. Editor owns click and selection policy. Tests, documentation and build declarations will be updated. The first version targets static CPU mesh geometry; shader alpha discard, deformation, GPU picking, physics queries and multi-selection are outside this change. No commit is requested.
