## Why

Editor currently saves depth-convention changes for the next launch even though its renderer already supports convention-specific views and resource generations. Live switching will let users compare and debug Standard-Z and Reversed-Z without restarting or reopening their scene.

## What Changes

- Apply Render settings depth changes to the next rendered frame through the shared GUI/automation settings service.
- Use the committed convention for scene rendering, camera previews, picking, placement, debug geometry, frozen culling and three-dimensional asset previews.
- Preserve independent explicit settings persistence, scene/asset history, and GPU resource retirement.
- **BREAKING**: `render.settings.get/set` operation version 2 reports the committed live convention instead of a startup-frozen value. IDs and wire fields remain stable; `set` completes at Main commit, not presentation. Saved settings retain version 1 and their existing format.
- Add same-session switching, GUI parity, GPU pixel/resource and lifecycle regression coverage and update user documentation.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `depth-conventions`: Replace startup-only Editor selection with live, frame-consistent scene and asset preview switching and explicit persistence/completion semantics.

## Impact

Editor settings/view construction and asset window frame arguments, reflected rendering metadata, automation operation descriptions/version, rendering and Editor tests, and depth/render/automation documentation. Reuse renderer target generations and convention-aware caches; no new dependencies, plugin lifecycle, transport branches or native RHI interfaces. Direct swapchain FrameDepth callers retain their existing optimized-clear contract. Do not archive the change or commit code as part of this work.
