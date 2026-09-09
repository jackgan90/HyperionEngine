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

### Requirement: Instance batching controls and diagnostics
Scene Viewer SHALL render eligible repeated models through the generic instance batch system by default and expose runtime and command-line controls for count-one comparison. Diagnostics SHALL distinguish source visible items, actual draws, instanced draws, submitted instances, fallback reasons, chunk reuse/rebuilds, uploads and planning/preparation timings.

#### Scenario: Toggle batching
- **WHEN** batching is toggled for an unchanged scene and view
- **THEN** rendered content and visible-item coverage remain unchanged while actual draw counts reflect the selected mode

### Requirement: Reproducible batching validation and performance
Delivery SHALL include deterministic CPU/GPU regression and measured repeated A/B performance on the existing scene. Tests SHALL cover visibility changes, differing numeric values, incompatible resources/state, chunk limits, group failures and old-frame lifetime. Measurements SHALL record build/device/configuration, warmup, frame mean/P95 and distinguish CPU/GPU scopes from total frame time.

#### Scenario: Default repeated scene
- **WHEN** the shipped Showcase scene is rendered after loading with batching enabled and disabled
- **THEN** equivalent images are validated and eligible draws reduce through instancing rather than fewer visible items

#### Scenario: Static and moving measurements
- **WHEN** repeated warmed static and moving-camera A/B runs are collected serially
- **THEN** results report draw reduction, cache/upload behavior and performance including regressions and measurement limitations

### Requirement: Shadow demonstration and controls
SceneViewer SHALL demonstrate directional shadows on its ground and models, expose enable/light direction/resolution/distance/bias and cascade visualization controls, and show per-view draw/culling and shadow timing diagnostics using engine GUI APIs.

#### Scenario: Inspect shadows interactively
- **WHEN** the user changes the light direction or moves/hides a model
- **THEN** all active cascades update on the next frame and the visible scene and diagnostics reflect the change

### Requirement: Shadow validation fixtures
Delivery SHALL include deterministic contact, sloped, masked, thin/double-sided, offscreen-caster, cascade-transition and extreme-light fixtures plus static/moving performance comparisons.

#### Scenario: Shadow correctness suite
- **WHEN** CPU, GPU and viewer shadow acceptance tests run
- **THEN** depth sampling, main/shadow view separation, shadow reception, empty-map clearing and enabled/disabled/batching comparisons are verified with documented image and performance evidence
