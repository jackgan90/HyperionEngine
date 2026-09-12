## MODIFIED Requirements

### Requirement: Material interpretation
The engine SHALL render supported metallic-roughness material maps and alpha/double-sided modes using role-appropriate color spaces. Offline import SHALL convert embedded glTF material data into general reflected material assets, preserving supported UV/mip/sampler behavior. Native model rendering SHALL resolve material and texture references and honor their authored pass descriptions. Camera, object and PBR values SHALL use the generic parameter system. In logical-scene rendering, builtin lighting constants SHALL derive from the selected scene light nodes; ModelViewer initialization SHALL create actual default nodes matching the established initial values rather than depending on persistent session-owned lighting.

#### Scenario: Material interpretation acceptance
- **WHEN** a fixture combines textured opaque, masked and blended primitives
- **THEN** their colors, depth behavior and material bindings match expected semantics

#### Scenario: PBR migration parity
- **WHEN** existing fixtures exercise unlit, normal maps, UV1, role color spaces, mip generation, mirrored transforms and double-sided surfaces
- **THEN** the independent asset path preserves the established pixel results and validation behavior

#### Scenario: Default ModelViewer lighting
- **WHEN** ModelViewer initializes its temporary logical scene
- **THEN** actual scene camera/light nodes reproduce its established initial camera and lighting behavior and subsequent controls edit those nodes
