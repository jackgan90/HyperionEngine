## Why

The Editor supports individual and Ctrl-toggle selection but lacks scene-wide Ctrl+A and contiguous Outliner selection. These gestures must preserve shared GUI/automation state and the existing hierarchy drag, text-input and viewport ownership contracts.

## What Changes

- Add Ctrl+A in focused Viewport and Outliner panels to select all logical scene nodes, including collapsed, filtered, hidden and disabled nodes.
- Add Outliner Shift-click ranges in displayed row order; Ctrl+Shift appends a range. Keep a fixed range anchor across consecutive Shift clicks and make the endpoint primary.
- Treat Viewport Shift-click exactly like Ctrl-click, capturing modifiers at press.
- Add shared validated select-all and batch selection, with a discoverable typed `scene.selection.select_all` operation and a bounded summary result. Expand explicit selection beyond the unrelated 128-object edit-batch limit without changing existing operation IDs or field semantics.
- Preserve document revision, dirty state and history during selection; cover focus, text input, cancellation, stale handles, dragging and large selections.

## Capabilities

### New Capabilities

- `editor-selection-shortcuts`: Focus-aware scene-wide selection and Outliner contiguous selection using the shared SceneEditing domain.

### Modified Capabilities

- `editor-viewport-picking`: Shift captured at press has the same toggle, miss and unavailable behavior as Ctrl.

## Impact

Affected modules are Runtime/Gui, Runtime/SceneEditing, Plugins/Editor and Plugins/Automation, their existing tests, and Editor/Automation documentation. No application-host, renderer query, plugin selection or persistence-format change is required. The completed change remains active and uncommitted as requested.
