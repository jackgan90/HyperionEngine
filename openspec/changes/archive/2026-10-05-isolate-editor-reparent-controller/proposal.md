## Why

Hierarchy drag state, drop delivery, cancellation and parent expansion are currently spread across FEditorPlugin fields and methods. Their common lifetime and reset requirements need one explicit private owner so input routing, document replacement and shutdown cannot retain parallel gesture state.

## What Changes

- Move hierarchy gesture state, input routing, preview, delivery, cancel/reset and post-drop expansion into a private FEditorReparentController.
- Model no delivery, root delivery and node delivery using named alternatives.
- Retain the existing SceneEditing document, selection, history, validation and ReparentSceneNodes operation; use narrow transient coordination calls for Inspector/selection/navigation.
- Delegate from the existing plugin and expose read-only gesture observations for existing acceptance exercises.
- Document lifetime/ownership and validate the existing interactions and document/content shutdown boundaries.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `editor-hierarchy-drag-drop`: add the internal ownership and invalidation contract while preserving all existing behavior.

## Impact

Private Editor controller, composition and gesture call sites, existing acceptance observations and focused regression coverage. No service provider, plugin dependency, automation ID, persistence schema or renderer behavior changes. Human acceptance is required after this change and before Change 3.
