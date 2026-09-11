## MODIFIED Requirements

### Requirement: Manifest-based multi-model viewer
SceneViewer SHALL delegate native scene loading, instance bookkeeping, Tick synchronization and shutdown to the Renderer scene runtime entity. It SHALL load reflected versioned .hasset scenes with native asset references, unique asset/instance IDs, transforms, visibility and initial camera settings. Instances SHALL share assets and GPU resources. Loading SHALL remain asynchronous with independent instance failures and generation-safe cancellation. Existing camera, controls, GUI, fixed-step demo animation and plugin IDs SHALL remain unchanged. CLI/configuration keys SHALL remain stable with native asset paths replacing source-format paths.

#### Scenario: Repeated and failed assets
- **WHEN** multiple instances share a valid asset and another asset fails
- **THEN** valid models render with independent transforms and the failed entry reports an error without stopping them

#### Scenario: Existing viewer behavior
- **WHEN** SceneViewer and ModelViewer run their acceptance cases with imported native content
- **THEN** rendering, material appearance, input, manipulation, UI and capture behavior have no regressions

## ADDED Requirements

### Requirement: Save current scene
SceneViewer SHALL expose asynchronous saving of current editable scene state and camera to .hasset, with visible success/failure status and no frame-loop wait. Saving SHALL preserve valid references when the output location changes.

#### Scenario: Viewer save and reload
- **WHEN** the user edits a scene, saves it and loads the result
- **THEN** current instance state and camera are restored and the Viewer remains responsive during saving
