## ADDED Requirements

### Requirement: Authoritative GBuffer attachment roles
Renderer SHALL own one explicit correspondence between GBuffer roles, physical slots, binding semantics, default formats and per-role format constraints. Allocation, layout validation, lighting, debug and contact-shadow consumers SHALL use that contract. Existing BaseMetallic, Normals, Surface and Emissive packing, GBuffer0..3 bindings, SV_Target0..3 order and resource/cache lifetime behavior SHALL remain compatible.

#### Scenario: Role-specific consumers
- **WHEN** a contact-shadow pass requests normals and surface data or a local-light pass requests its material inputs
- **THEN** it selects named roles through the shared mapping and receives the same existing textures and semantics

#### Scenario: Layout creation and validation
- **WHEN** default, high-precision or invalid GBuffer formats are configured
- **THEN** defaults and role constraints come from the shared contract, supported layouts preserve existing byte costs and output, and unsupported role formats or device capabilities are rejected

#### Scenario: Resize and route changes
- **WHEN** size, precision, depth convention or Forward/Deferred selection changes
- **THEN** resources, target signatures, shader bindings and prior in-flight lifetimes retain their existing behavior
