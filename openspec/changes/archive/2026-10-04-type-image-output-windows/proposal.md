## Why

Screenshot admission, frame routing and asset-window closure compare repeated `main`/`assets` strings. Typed window identity removes this coupling while preserving the existing open string protocol and validation order.

## What Changes

- Renderer owns known screenshot window kinds and their wire mapping.
- The request's window value retains unknown strings losslessly; known native values use an enum.
- Editor admission, polling, rendering and asset-window closure use typed identity.
- Preserve default main routing, schemas, errors, readback, file writes and completion behavior.

## Capabilities

### New Capabilities
- `typed-image-output-windows`: Typed screenshot destinations with compatible wire values and lifecycle behavior.

### Modified Capabilities
None.

## Impact

Renderer RenderOutput values/reflection, Editor screenshot consumers, and focused native/protocol tests. No new operations, transport branches, plugin dependencies or capture algorithms. RenderDoc exercise options are outside this change.
