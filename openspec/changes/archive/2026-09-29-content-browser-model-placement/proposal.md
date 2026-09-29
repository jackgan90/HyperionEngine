## Why

Content Browser already publishes asset drag payloads, but the viewport only accepts registered presets. Native models should be placeable through the same preview, positioning, history and persistence path without duplicating the existing Place Object behavior.

## What Changes

- Accept native model assets dragged from Content Browser into the viewport.
- Unify preset and model candidates, resource preparation, preview and commit logic.
- Prepare models asynchronously; releasing before a valid preview cancels without creating a node.
- Create one model node retaining internal instances and source materials, with shared undo/redo and save/reload behavior.
- Add a reflected `scene.placement.place_model` operation using the same domain implementation, preserving existing placement contracts.
- Cover cancellation, stale documents/content, invalid assets and preset regressions.
- Prefer ready source materials during native model preview, with per-section shaded fallback and ordinary scene lighting/pass selection.

## Capabilities

### New Capabilities
- `content-model-placement`: Native model placement from Content Browser and automation through shared preparation and transactions.

### Modified Capabilities

None. Existing preset placement and generic asset drag contracts remain compatible.

## Impact

Editor placement orchestration, SceneEditing request/interface reflection, automation registration, desktop acceptance and documentation. Reuses Renderer placement calculations, native model loading and transient geometry; no new third-party dependency, application lifecycle or serialized scene format. The completed change remains active and all code remains uncommitted as requested.
