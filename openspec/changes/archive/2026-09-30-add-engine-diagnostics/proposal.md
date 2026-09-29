## Why

The Editor Log now retains engine output, but several important failures are only stored in task/document status and cannot be reconstructed from the log. Diagnostics must expose meaningful outcomes without introducing per-frame noise.

## What Changes

- Add contextual diagnostics at plugin activation/cleanup, content-root transitions, import completion, document save completion and scene loading/refresh failure boundaries.
- Include operation, path or identity, cause and recovery/outcome in messages; retain existing exception and result behavior.
- Surface D3D12 validation warnings and errors without repeating the same diagnostic on every statistics query.
- Document sparse logging rules and validate terminal-event logging, failure context and quiet polling.

## Capabilities

### New Capabilities
- `engine-diagnostics`: Sparse, contextual lifecycle and terminal-operation diagnostics shared by GUI and automation consumers.

### Modified Capabilities
None.

## Impact

Runtime Application/Plugins, Content, AssetImport, AssetEditing, SceneEditing, Shaders and Renderer; ApplicationServices recovery; private D3D12 validation collection; targeted tests and logging guidance. Existing Core Log and application.log.read are reused. Plugins and AssetEditing explicitly declare their private Core logging dependency. No new transport branches, operations, public domain contracts or third-party dependencies are required.
