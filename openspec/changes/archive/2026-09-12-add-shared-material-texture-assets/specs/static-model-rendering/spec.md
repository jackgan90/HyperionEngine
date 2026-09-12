## MODIFIED Requirements

### Requirement: Material interpretation
The engine SHALL render supported metallic-roughness material maps and alpha/double-sided modes using role-appropriate color spaces. Offline import SHALL convert embedded glTF material data into general reflected material assets, preserving supported UV/mip/sampler behavior. Native model rendering SHALL resolve material and texture references and honor their authored pass descriptions. Camera, object and PBR values SHALL use the generic parameter system; builtin lighting constants SHALL come from the documented Scene provider with current default values.

#### Scenario: Material interpretation acceptance
- **WHEN** a fixture combines textured opaque, masked and blended primitives
- **THEN** their colors, depth behavior and material bindings match expected semantics

#### Scenario: PBR migration parity
- **WHEN** existing fixtures exercise unlit, normal maps, UV1, role color spaces, mip generation, mirrored transforms and double-sided surfaces
- **THEN** the independent asset path preserves the established pixel results and validation behavior

## ADDED Requirements

### Requirement: Shared native model resources
Renderer SHALL reuse CPU texture sources, GPU textures and material definitions across different model assets that reference the same immutable dependencies. Old versions SHALL remain alive through existing submission fences. Readiness and failures SHALL include material and texture dependencies.

#### Scenario: Two model assets share dependencies
- **WHEN** independently loaded models reference one material and texture revision
- **THEN** diagnostics prove one shared dependency preparation/upload and both models render correctly

#### Scenario: Missing material dependency
- **WHEN** a model's pinned material or texture dependency is missing or mismatched
- **THEN** loading reports the dependency failure without publishing an incomplete model
