## ADDED Requirements

### Requirement: Editor interaction
The engine SHALL keep frame/input processing active during asynchronous loading and display terminal failures.

#### Scenario: Editor interaction acceptance
- **WHEN** a model load is delayed or a dependency fails
- **THEN** the window remains responsive and reports loading or the failure

### Requirement: Preserved model asset capabilities
The migration SHALL preserve supported glTF material behavior, interactive orbit/zoom/fit, asynchronous loading/error display, and native asset identity, material semantics and resource lifetime contracts. CPU model assets SHALL remain usable independently of render registration.

#### Scenario: Editor model asset workflow
- **WHEN** a native model is opened in the Editor asset workspace
- **THEN** loading, camera controls, material/capture validation and terminal error reporting continue to behave as documented

## MODIFIED Requirements

### Requirement: Material interpretation
The engine SHALL render supported metallic-roughness material maps and alpha/double-sided modes using role-appropriate color spaces. Offline import SHALL convert embedded glTF material data into general reflected material assets, preserving supported UV/mip/sampler behavior. Native model rendering SHALL resolve material and texture references and honor their authored pass descriptions. Camera, object and PBR values SHALL use the generic parameter system. In logical-scene rendering, builtin lighting constants SHALL derive from the selected scene light nodes rather than depending on persistent session-owned lighting.

#### Scenario: Material interpretation acceptance
- **WHEN** a fixture combines textured opaque, masked and blended primitives
- **THEN** their colors, depth behavior and material bindings match expected semantics

#### Scenario: PBR migration parity
- **WHEN** existing fixtures exercise unlit, normal maps, UV1, role color spaces, mip generation, mirrored transforms and double-sided surfaces
- **THEN** the independent asset path preserves the established pixel results and validation behavior

#### Scenario: Scene-owned default lighting
- **WHEN** legacy content is converted into a logical scene
- **THEN** actual light nodes preserve authored lighting and subsequent controls edit those nodes

### Requirement: All current scene plugins use generic materials
Triangle and Editor SHALL render through the new material system. Triangle SHALL use a zero-texture material, and scene and asset rendering SHALL share the Runtime model-to-material adapter. DebugUI SHALL use the new RHI binding/packet contracts while keeping its overlay behavior. Generic Renderer and D3D12 draw paths SHALL no longer contain the old fixed model layout branch.

#### Scenario: Existing plugin acceptance
- **WHEN** shared primitive, model rendering and Editor acceptance workflows run
- **THEN** configuration, loading, camera/scene controls, clipping, capture and expected pixels remain valid through the new binding path

## REMOVED Requirements

### Requirement: Viewer interaction
**Reason**: The independent application contract is retired; shared capabilities are owned by Editor and Runtime.
**Migration**: Use the Editor workflows and shared services specified by this change.

### Requirement: Preserved viewer and asset interfaces
**Reason**: The independent application contract is retired; shared capabilities are owned by Editor and Runtime.
**Migration**: Use the Editor workflows and shared services specified by this change.
