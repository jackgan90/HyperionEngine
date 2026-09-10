## MODIFIED Requirements

### Requirement: Manifest-based multi-model viewer

SceneViewer SHALL delegate scene loading, instance bookkeeping, Tick synchronization and shutdown to the Renderer scene runtime entity. It SHALL load the existing validated versioned manifest with relative asset paths, unique asset/instance IDs, TRS, visibility and initial camera settings. Instances SHALL share assets and GPU resources. Loading SHALL remain asynchronous with independent instance failures and generation-safe cancellation. Existing camera, controls, GUI, fixed-step demo animation, plugin IDs and CLI behavior SHALL remain unchanged.

#### Scenario: Repeated and failed assets
- **WHEN** multiple instances share a valid asset and another asset fails
- **THEN** valid models render with independent transforms and the failed entry reports an error without stopping them

#### Scenario: Existing viewer behavior
- **WHEN** SceneViewer and ModelViewer run their prior acceptance cases after the refactor
- **THEN** loading, rendering, material appearance, input, manipulation, UI and capture behavior have no regressions
