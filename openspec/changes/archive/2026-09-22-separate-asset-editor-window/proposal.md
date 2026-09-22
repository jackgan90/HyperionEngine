## Why

Asset tabs currently replace the scene viewport and its Details panel. Developers need to inspect or edit an asset while observing the scene that consumes it.

## What Changes

- Open non-scene assets in one independent native Asset Editor window containing multiple asset tabs, previews and asset properties.
- Keep the main scene viewport, Outliner and scene Details available independently.
- Route input and history to their originating window; preserve save-triggered consumer refresh.
- Handle independent resize and asset minimization, owner-group minimize/restore, close/cancel/save, recreation and application shutdown using existing plugin ownership.
- Retain existing asset formats, authoring capabilities and third-party code. Arbitrary tab tear-off and cross-window docking are outside this change.

## Capabilities

### New Capabilities
- `asset-editor-window`: Separate native asset workspace with independent input, rendering and lifecycle.

### Modified Capabilities

None. This extends the unarchived `add-native-asset-editors` implementation and supersedes its scene/asset tab container arrangement.

## Impact

Editor plugin window composition and acceptance tests; small engine-owned Platform/Gui API extensions as needed. The secondary window shares existing assets, device and resource services. Runtime/Application and third-party libraries remain unchanged. No commit or push is requested.
