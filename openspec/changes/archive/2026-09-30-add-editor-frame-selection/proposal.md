## Why

Editor can frame the entire scene, but cannot quickly navigate to an object selected in the viewport or Outliner. A shared selection-framing operation makes large scenes easier to edit and replaces the dedicated Frame Scene toolbar button with the requested F shortcut.

## What Changes

- Add F to frame the current ordered scene selection in the independent editor browsing view.
- Center the camera on the union of selected world-space bounds, preserve its orientation and field of view, and fit the selection to the viewport aspect ratio.
- Include selected subtrees without counting overlapping ancestor/descendant selections twice; provide stable framing for lights, cameras and empty groups.
- Respect scene-panel focus, text ownership, popups, active gestures, scene readiness and strict camera-preview isolation.
- Remove the main toolbar Frame Scene button while retaining Home and the existing `view.frame_scene` operation.
- Add discoverable `view.frame_selection` through the same UI-independent viewport service as the GUI, with existing reflected document/revision and viewport-state values.
- Cover geometry, selection, GUI input and automation equivalence; update Editor and automation documentation.

## Capabilities

### New Capabilities

- `editor-selection-framing`: Shared selection bounds, browsing-camera fitting, focus-aware F interaction and typed automation parity.

### Modified Capabilities

None. Existing temporary-navigation, authored-camera, Home and automation contracts remain intact.

## Impact

- Runtime/Renderer: reusable bounds framing, selected-subtree bounds and the `ISceneViewport` operation contract.
- Runtime/Platform and Runtime/Gui private adapters: engine F key and SDL/ImGui mappings.
- Plugins/Editor: shared viewport provider, shortcut routing, toolbar and acceptance exercises.
- Plugins/Automation: typed registration only; no transport or application-host changes.
- Renderer/GUI/Editor/automation tests and documentation; no dependency additions or asset migrations.
- This change remains active after implementation and testing; archive and Git commit are explicitly outside this delivery.
