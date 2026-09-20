## Why

The editor can inspect and transform existing scene objects but cannot place new primitives or lights from a palette. A reusable drag-and-drop placement path is needed before adding broader asset and object authoring workflows.

## What Changes

- Add Window > Place Object and a searchable, dockable palette with many-to-many object/category registration.
- Ship Cube, Sphere, Cylinder, Cone and Plane as native Engine model assets with a shared default material, plus generated light icon resources.
- Add engine-owned GUI drag/drop contracts, reusable viewport placement calculations and transient mesh/icon previews.
- Add dynamic scene model asset registration and instantiation, preserving immutable loaded manifests and native save/reload references.
- Commit one undoable creation on a valid drop; cancellation leaves scene, history and selection unchanged. Support placement and Save As in the initial empty document.
- Display and pick PointLight, DirectionalLight and SpotLight icons, with directional cues and explicit main-directional-light selection.

## Capabilities

### New Capabilities
- `object-placement`: Palette registration, drag sessions, placement, preview isolation, commit/cancel and light visualization.
- `gui-object-drag-drop`: Engine-owned payload, source, target, delivery and cancellation contracts.

### Modified Capabilities
- `scene-runtime-instance`: Dynamic native model registration and saveable instantiation in loaded and empty scenes.
- `scene-ray-queries`: World-space geometric surface normal for placement queries.

## Impact

Changes affect Plugins/Editor, Runtime/Gui, Runtime/Renderer, Runtime/Scene, Content, asset generation tools, documentation and targeted tests. GUI vendor calls stay in private adapters; Renderer consumes owned immutable preview snapshots; Runtime/Application and native backends retain their existing responsibilities. No new third-party dependencies or scene archive format break is required. This work is not to be committed automatically.
