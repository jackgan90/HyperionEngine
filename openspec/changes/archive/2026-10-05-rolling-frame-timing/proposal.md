## Why

Editor profiling HUD currently retains one frame interval sampled every 100 ms. Real occasional long frames can therefore dominate the displayed value, while intervening frames are omitted, making the number a poor estimate of sustained frame rate.

## What Changes

- Collect every Editor update interval into a bounded rolling window covering approximately one second.
- Show average interval and throughput FPS alongside last, P95, maximum and explicitly thresholded long-frame counts.
- Add reflected timing-window metadata to the shared render diagnostics result while preserving the existing raw frame-interval field and operation identity.
- Document the measurement boundary and validate both statistical calculations and GUI/automation integration.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `editor-render-diagnostics`: Every-frame rolling frame statistics shared by the overview HUD and `render.statistics`.

## Impact

Runtime/Renderer gains a reusable frame timing accumulator and reflected result. Editor owns collection on Main and consumes the shared snapshot in its HUD. Automation inherits the additive result through its existing operation adapter. Renderer scheduling, VSync and GPU timings are unchanged. No new dependencies or persistent settings are introduced.
