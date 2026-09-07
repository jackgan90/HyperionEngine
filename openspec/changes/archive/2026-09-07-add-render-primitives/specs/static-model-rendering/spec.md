## MODIFIED Requirements

### Requirement: Static model presentation
The engine SHALL render loaded static glTF geometry with node transforms, a usable camera and depth testing through generic Runtime render primitives. A Main model instance SHALL be a logical group rather than a GPU submission unit. Initial primitive mapping SHALL distinguish each selected-scene node occurrence and referenced glTF primitive/section while allowing geometry resources to be shared.

#### Scenario: Static model presentation acceptance
- **WHEN** a model with overlapping surfaces is opened
- **THEN** the model fits the initial view and nearer surfaces occlude farther ones

#### Scenario: Multiple nodes and model instances
- **WHEN** two independently positioned models use an asset whose nodes reference shared geometry
- **THEN** each node/section occurrence has the correct instance transform, with shared geometry and independent primitive state

### Requirement: Nonblocking upload publication
The engine SHALL retain staging and destination resources through necessary GPU completion and publish only ready model resource groups without per-texture idle waits. Readiness SHALL be published to Render without borrowing the Main model. Removing a loading model SHALL prevent its late preparation or upload result from registering visible primitives.

#### Scenario: Nonblocking upload publication acceptance
- **WHEN** model textures upload while frames continue
- **THEN** rendering never references incomplete or freed GPU resources

#### Scenario: Model removed while loading
- **WHEN** a model is removed before its upload completes
- **THEN** frame/input processing continues, no late result makes the removed model visible, and its resource request is completed or retired safely

## ADDED Requirements

### Requirement: Cross-model rendering correctness
Multiple model instances SHALL participate in scene-wide depth and transparency organization. Opaque and masked draws SHALL retain the existing depth/material semantics. Blended primitives SHALL be sorted across model boundaries by the documented stable projected-center depth policy, preserving its existing limitation for intersecting transparent geometry.

#### Scenario: Interleaved transparent models
- **WHEN** transparent sections from different models are interleaved in view depth
- **THEN** the rendered pixel result follows global back-to-front section order rather than model registration order

#### Scenario: Independent instance update
- **WHEN** one instance sharing an asset is moved, hidden or given a material override
- **THEN** the other instance retains its previous transform, visibility and material behavior

### Requirement: Preserved viewer and asset interfaces
The migration SHALL preserve supported glTF material behavior, interactive orbit/zoom/fit, asynchronous loading/error display, existing CLI and serialized configuration fields, plugin IDs, executable names and existing build target names. CPU model assets SHALL remain usable independently of render registration.

#### Scenario: Existing model viewer workflow
- **WHEN** existing model-viewer configuration, CLI and acceptance fixtures run after migration
- **THEN** loading, camera controls, material/capture validation and terminal error reporting continue to behave as documented
