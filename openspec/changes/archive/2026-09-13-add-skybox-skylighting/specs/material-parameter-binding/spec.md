## ADDED Requirements

### Requirement: Typed cube texture resources
Shader reflection, material values and persistence, resource preparation and RHI bindings SHALL distinguish TextureCube from Texture2D, preserving existing enum values and block layouts. DXIL and SPIR-V reflection SHALL agree on cube resource dimension. Actual resource dimensions SHALL be validated against declared bindings.

#### Scenario: Cube resource mismatch
- **WHEN** a 2D texture is assigned to a cube shader binding or the converse
- **THEN** preparation or value validation rejects the mismatch explicitly before drawing

#### Scenario: Shared cube resource
- **WHEN** multiple sky/material consumers reference one native cube revision
- **THEN** they reuse its immutable CPU source and GPU texture while owning independent sampler bindings
