# scene-viewer Specification

## Purpose
Define manifest-based multi-model viewing, runtime scene inspection and diagnostics while preserving existing ModelViewer behavior.

## Requirements
### Requirement: Manifest-based multi-model viewer
SceneViewer SHALL load a validated versioned scene manifest with relative asset paths, unique asset/instance IDs, TRS, visibility and initial camera settings. Instances SHALL share assets and GPU resources. Loading SHALL remain asynchronous with independent instance failures and generation-safe cancellation.

#### Scenario: Repeated and failed assets
- **WHEN** multiple instances share a valid asset and another asset fails
- **THEN** valid models render with independent transforms and the failed entry reports an error without stopping them

### Requirement: Scene inspection controls
SceneViewer SHALL provide navigable camera and fit, culling mode selection, freeze-culling view, optional bounds visualization, runtime instance manipulation and separate model/primitive/item/draw and timing diagnostics through engine GUI interfaces.

#### Scenario: Frozen culling view
- **WHEN** the user freezes culling and moves the display camera
- **THEN** the frozen view continues controlling spatial rejection and statistics while the display uses the current camera

#### Scenario: Runtime manipulation
- **WHEN** a displayed instance is added, removed, moved or hidden
- **THEN** scene rendering and diagnostics reflect the change without restarting the viewer

### Requirement: Existing model viewer compatibility
ModelViewer SHALL use the new logical scene attachment while preserving its existing plugin ID, CLI, camera, loading and material behavior.

#### Scenario: Existing model acceptance
- **WHEN** the existing ModelViewer acceptance suite runs
- **THEN** prior loading, input, capture and rendering cases continue to pass
