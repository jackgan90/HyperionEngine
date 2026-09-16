## Why

Hyperion currently exposes fixed debug panels over a full-window scene. A dedicated editor needs a dockable workspace and an embedded scene viewport while preserving engine ownership of rendering and reusable camera controls.

## What Changes

- Add a standalone `hyperion_editor` application with a compact dark UE-inspired workspace, menus, dockable viewport, Outliner, Details and scene browser.
- Open native scenes from mounted content through File > Open Scene, including Sponza; report loading and failures in the editor.
- Reuse the Runtime scene instance and camera controller for viewport navigation.
- Pin ImGui Docking and extend the private GUI adapter with dock/layout/image support.
- Extract reusable engine-owned GUI rendering from DebugUI and support sampled scene output through RenderGraph.
- Add focused integration coverage and portable launch documentation. Leave changes uncommitted.

## Capabilities

### New Capabilities
- `scene-editor`: Standalone docked editor, scene opening, embedded rendering and camera input routing.
- `gui-texture-rendering`: Engine-owned GUI texture commands and reusable rendering.

### Modified Capabilities

None. Existing Viewer defaults and interfaces remain compatible.

## Impact

Gui, Renderer, a new GuiRenderer runtime module, DebugUI, a new Editor application, the ImGui dependency lock, CMake, tests and documentation. Scene and Environment remain CPU-only. First-version scope is viewing and inspecting scenes in one operating-system window; asset authoring, undo/redo and detached OS windows are deferred.
