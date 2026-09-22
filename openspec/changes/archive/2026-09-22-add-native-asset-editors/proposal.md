## Why

Content Browser currently opens only scene hassets. Developers need to inspect and edit the existing texture, model, sky and material assets independently, with reliable history and saved changes reflected in their scene. The obsolete persistent Catalog type must be removed instead of receiving a new editor.

## What Changes

- **BREAKING** Remove `hyperion.assetcatalog` persistence and its CLI creation command; feed discovered entries directly into a non-persistent asset index.
- Add multiple asset editor tabs with independent drafts, previews, save state and undo/redo, retaining the existing single scene document.
- Provide texture channel/mip/cube/HDR inspection and supported encoding edits; model statistics, node transforms and material slots; sky lighting inspection; generic material parameter, texture and sampler editing.
- Add typed selection of existing asset references and controlled diagnostics for unavailable or invalid assets.
- Isolate unsaved edits to their document. After successful saves, refresh affected open consumers without losing scene edits, selection or history.
- Preserve asset IDs, validate saves against their loaded baseline, and retire asynchronous/GPU work safely on close or content-root changes.

## Capabilities

### New Capabilities
- `native-asset-editors`: Multiple typed asset documents, previews, property editing, history and reference selection.
- `saved-asset-refresh`: Saved dependency publication and refresh of live consumers while preserving authored state.

### Modified Capabilities
- `editor-content-browser`: Dispatch supported types to editors and protect all documents during root changes.
- `native-asset-registry`: Remove persistent Catalog assets and retain a discovered in-memory index.
- `offline-asset-import`: Remove Catalog creation from the CLI contract.

## Impact

Editor and Gui, Assets/AssetTypes, Renderer, texture/material authoring helpers, AssetTool, existing integration tests, and current documentation. No material/shader graph, shader editor, topology/LOD/collision authoring, sky rebaking, or Git commit is included. Existing plugin ownership and native backend boundaries remain unchanged.
