## MODIFIED Requirements

### Requirement: Static model presentation
The engine SHALL render loaded static glTF geometry with node transforms, a usable camera and default depth testing through generic Runtime render primitives. A Main model instance SHALL be a logical group rather than a GPU submission unit. Initial primitive mapping SHALL distinguish each selected-scene node occurrence and referenced glTF primitive/section while allowing geometry resources to be shared. Depth testing SHALL come from the default PBR material pass; an explicitly selected compatible material SHALL control its own depth and resource state independently of Model.

#### Scenario: Static model presentation acceptance
- **WHEN** a model with overlapping surfaces is opened
- **THEN** the model fits the initial view and nearer surfaces occlude farther ones

#### Scenario: Multiple nodes and model instances
- **WHEN** two independently positioned models use an asset whose nodes reference shared geometry
- **THEN** each node/section occurrence has the correct instance transform, with shared geometry and independent primitive state

#### Scenario: Replace default model material
- **WHEN** a model uses a custom material with a different texture count or disabled depth
- **THEN** the generic renderer honors the compatible selected material without reintroducing PBR layout or mandatory depth behavior

### Requirement: Material interpretation
The engine SHALL render supported metallic-roughness material maps and alpha/double-sided modes using role-appropriate color spaces. A Renderer-owned adapter SHALL convert existing FModelMaterial CPU data into builtin generic PBR definitions/instances, preserving serialized fields and supported UV/mip/sampler behavior. Camera, object and PBR values SHALL use the generic parameter system; builtin lighting constants SHALL come from the documented Scene provider with current default values.

#### Scenario: Material interpretation acceptance
- **WHEN** a fixture combines textured opaque, masked and blended primitives
- **THEN** their colors, depth behavior and material bindings match expected semantics

#### Scenario: PBR migration parity
- **WHEN** existing fixtures exercise unlit, normal maps, UV1, role color spaces, mip generation, mirrored transforms and double-sided surfaces
- **THEN** the migrated generic material path preserves the established pixel results and validation behavior

## ADDED Requirements

### Requirement: All current scene plugins use generic materials
Triangle, ModelViewer and SceneViewer SHALL render through the new material system. Triangle SHALL use a zero-texture material, and the two viewers SHALL share the Runtime model-to-material adapter. DebugUI SHALL use the new RHI binding/packet contracts while keeping its overlay behavior. Generic Renderer and D3D12 draw paths SHALL no longer contain the old fixed model layout branch.

#### Scenario: Existing plugin acceptance
- **WHEN** existing triangle, model-viewer, scene-viewer and GUI acceptance workflows run
- **THEN** configuration, loading, camera/scene controls, clipping, capture and expected pixels remain valid through the new binding path
