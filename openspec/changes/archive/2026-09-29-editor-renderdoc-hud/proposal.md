## Why

RenderDoc's default overlay obscures the Editor menu and viewport. Users need to hide it while retaining frame capture.

## What Changes

- Add a persistent Show RenderDoc HUD preference, defaulting to false for Editor, applied at startup and live without restart.
- Encapsulate overlay control in Runtime/Capture and expose shared Editor domain operations to GUI and typed automation.
- Preserve capture selection, capture/replay behavior, and explicitly disabled or unavailable providers.

## Capabilities

### New Capabilities

### Modified Capabilities

- `editor-preferences`: Independent persisted HUD visibility, backward-compatible defaults, live control and automation parity.
- `renderdoc-frame-capture`: Engine-owned overlay visibility query/control without changing capture operation.

## Impact

Runtime/Capture, Runtime/Renderer control interfaces, Editor preferences/UI, Automation operation registration, focused tests and documentation. No new dependencies or transport changes.
