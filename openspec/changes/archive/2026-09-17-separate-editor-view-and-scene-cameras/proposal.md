## Why

The editor silently copies the default scene camera once, then navigates an independent view. A visible Main Camera therefore appears to control the viewport while subsequent edits and disabling it have no visible effect. Browsing, the saved opening view, and authored scene cameras need explicit contracts.

## What Changes

- Store an optional initial browsing view as scene tool metadata; navigation never updates it implicitly. Expose an explicit undoable command to set it from the editor view.
- Initialize Editor and SceneViewer navigation from that preset, or deterministic scene framing when absent, without requiring a scene camera object.
- Add explicitly selected scene-camera preview, return-to-editor view, and apply-editor-view-to-camera actions. Disabled, missing, or removed preview cameras do not silently fall back.
- Preserve optional authored Camera components and runtime default-camera selection. Permit creating an authored camera from the editor view.
- Explicitly migrate the shipped Sponza browsing-only camera into a view preset, preserving shared assets and camera framing. Old files remain readable without topology changes during load.

## Capabilities

### New Capabilities
- `scene-browsing-views`: Initial browsing presets, temporary viewport navigation, explicit camera preview and guarded asset migration.

### Modified Capabilities

## Impact

Scene manifest/reflection/settings, Renderer view requests and shared navigation helpers, Editor document history and UI, SceneViewer navigation, scene import defaults, validation and documentation, and Sponza's scene asset and publication metadata in HyperionAssets. No new third-party dependency, pilot mode, picture-in-picture, automatic node deletion, or git commit is included.
