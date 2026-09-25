## Why

The engine has no self-contained default environment and model/material previews lack a sky. Cloudy currently lives in Game content, preventing consistent previews without a selected asset root.

## What Changes

- Move Cloudy and its baked radiance/specular textures into Engine content, preserving native identities and updating all Game references and rebuild metadata.
- Add an explicit Editor action to use the default sky in the current scene, with a shared undoable domain operation exposed to automation.
- Use the built-in sky for transient model and material previews.
- Preserve license attribution and verify Engine-only loading, history, persistence and rendering.

## Capabilities

### New Capabilities
- `engine-default-sky`: Built-in Cloudy environment, cross-mount migration, scene assignment and preview integration.

### Modified Capabilities

None; the new capability extends existing editing contracts.

## Impact

Engine Content, HyperionAssets native references and metadata, Environment reference contract, SceneEditing transactions, Editor UI/previews, automation scene catalog and regression coverage. No transport changes, archive or git commits.
