## Why

SceneViewer translates by a fixed distance per key-down event, so holding a key depends on OS repeat delay and rate and visibly jumps. WASD/QE and frame-time movement make camera positioning intuitive and continuous.

## What Changes

- Add W/S forward/back along the view direction, A/D strafe, Q/E world down/up; keep arrows/Page keys as continuous aliases.
- Provide a reusable runtime camera controller covering WASDQE, RMB orbit and wheel dolly for future editor apps/plugins.
- Track held keys and integrate movement once per application frame, independently of OS repeat.
- Stop on release, focus loss, GUI keyboard capture and minimized frames; bound long-frame displacement and normalize combined movement.
- Preserve mouse orbit/dolly, fit, scene editing and ModelViewer behavior.

## Capabilities

### New Capabilities
- `scene-viewer-camera-motion`: Continuous keyboard camera translation and input lifecycle.

### Modified Capabilities

None. Existing scene-viewer navigation remains available with continuous aliases.

## Impact

Platform key translation, GUI key translation, SceneViewer controls/state, Viewer frame integration, camera control tests and SceneManagement documentation. No new dependencies or serialized changes. No git commit.
