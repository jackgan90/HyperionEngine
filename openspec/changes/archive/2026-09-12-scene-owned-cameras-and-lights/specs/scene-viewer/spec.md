## MODIFIED Requirements

### Requirement: Manifest-based multi-model viewer
SceneViewer SHALL delegate native scene loading, node bookkeeping, Tick synchronization and shutdown to the Renderer scene runtime entity. It SHALL load reflected versioned .hasset scenes with native asset references, unique node IDs, hierarchy, model transforms/visibility, cameras and lights. Instances SHALL share assets and GPU resources. Loading SHALL remain asynchronous with independent instance failures and generation-safe cancellation. Existing interaction gestures, GUI availability, fixed-step demo animation and plugin IDs SHALL remain compatible while authoritative camera/light state moves into Scene. CLI/configuration keys SHALL remain stable with native asset paths replacing source-format paths.

#### Scenario: Repeated and failed assets
- **WHEN** multiple instances share a valid asset and another asset fails
- **THEN** valid models render with independent transforms, camera/light nodes remain editable, and the failed entry reports an error without stopping them

#### Scenario: Existing viewer behavior
- **WHEN** SceneViewer and ModelViewer run their acceptance cases with imported native content
- **THEN** rendering, material appearance, input, manipulation, UI and capture behavior have no regressions

### Requirement: Scene inspection controls
SceneViewer SHALL provide a navigable logical node tree, typed node/property editing, camera navigation and fit, culling mode selection, freeze-culling view, optional model-bounds visualization and separate node/model/primitive/item/draw and timing diagnostics through engine GUI interfaces. Selection SHALL use stable generation-safe handles. The controls SHALL expose node creation, clearly distinguished subtree or keep-children deletion, parent selection, KeepLocal/KeepWorld reparenting, node enable and model-only visibility, camera lens/focus properties, light color/intensity/cast-shadows properties and scene default/main selections. All mutations SHALL use scene APIs.

#### Scenario: Frozen culling view
- **WHEN** the user freezes culling and moves the display camera
- **THEN** the frozen view continues controlling spatial rejection and statistics while display rendering, transparent sorting and CSM use the current scene camera

#### Scenario: Runtime manipulation
- **WHEN** a displayed node is added, removed, moved, disabled or reparented
- **THEN** its hierarchy, scene rendering and diagnostics reflect the change without restarting the viewer

#### Scenario: Inspect a camera and light after all models are removed
- **WHEN** the user removes every model but retains cameras, lights and groups
- **THEN** those nodes remain visible and editable in the tree, camera/light properties can be saved, and model add/fit controls handle the empty model collection safely

### Requirement: Existing model viewer compatibility
ModelViewer SHALL use a logical scene containing its model, camera and explicit default light nodes, while preserving its existing plugin ID, CLI, initial lens/fit, loading and material behavior. Input controls SHALL edit its scene camera and scene-frame submission SHALL use the same publication protocol as SceneViewer.

#### Scenario: Existing model acceptance
- **WHEN** the existing ModelViewer acceptance suite runs
- **THEN** prior loading, input, capture and rendering cases continue to pass through the scene-owned camera/light path

### Requirement: Shadow demonstration and controls
SceneViewer SHALL demonstrate directional shadows on its ground and models, expose scene-light direction/color/intensity/cast-shadows controls separately from pipeline shadow enable/resolution/distance/bias and cascade visualization controls, and show per-view draw/culling and shadow timing diagnostics using engine GUI APIs. Application GUI, explicit CLI light overrides and benchmark animation SHALL mutate the selected scene light rather than continuously setting session-owned builtin light parameters.

#### Scenario: Inspect shadows interactively
- **WHEN** the user changes the selected scene light direction or moves/hides a model
- **THEN** all active cascades update for the next submitted scene frame and the visible scene and diagnostics reflect the change

#### Scenario: Save GUI lighting changes
- **WHEN** light direction, intensity and cast-shadows state are edited through the viewer
- **THEN** a scene save persists those node values and a later reload restores them independently of application session defaults

### Requirement: Save current scene
SceneViewer SHALL expose asynchronous saving of current editable scene nodes, hierarchy, cameras, lights and persistent selections to .hasset, with visible success/failure status and no frame-loop wait. Saving SHALL use the runtime scene snapshot directly without plugin-supplied camera/light supplementation, and SHALL preserve valid references when the output location changes.

#### Scenario: Viewer save and reload
- **WHEN** the user edits a scene, saves it and loads the result
- **THEN** current model, hierarchy, camera, lighting and persistent selections are restored and the Viewer remains responsive during saving

#### Scenario: Failed save retains edits
- **WHEN** an asynchronous save fails
- **THEN** the viewer reports the error and preserves the current scene edits and selections for retry
