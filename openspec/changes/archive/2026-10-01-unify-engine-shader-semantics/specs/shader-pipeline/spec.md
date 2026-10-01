## ADDED Requirements

### Requirement: Structured resource element metadata
Shader reflection SHALL expose structured-buffer element metadata needed by engine resource contracts, including element stride and available numeric/structure/array member types and offsets. Reflection/cache versions SHALL change when this metadata changes. Native adapter types SHALL remain private.

#### Scenario: Structured light element reflection
- **WHEN** a shader uses a structured light record
- **THEN** the engine artifact carries sufficient element metadata to distinguish same-stride incompatible records

#### Scenario: Cached artifact after reflection change
- **WHEN** an artifact was produced under an older reflection metadata version
- **THEN** it is not reused as an artifact with the new element contract

### Requirement: Immutable generated shader declarations
Shader compilation SHALL support immutable virtual includes generated from engine parameter metadata. Include names and bytes SHALL participate in shader cache identity and remain owned throughout compilation. Generated declarations SHALL retain layout validation through native reflection.

#### Scenario: Changed generated declaration
- **WHEN** a generated uniform declaration changes while authored shader files remain unchanged
- **THEN** the old compiled artifact is not reused

#### Scenario: Generated layout mismatch
- **WHEN** an authored shader declaration conflicts with its expected engine contract
- **THEN** compilation or material preparation reports the mismatch before GPU submission

### Requirement: Generated uniform types for explicit shader resources
The declaration generator SHALL expose native HLSL uniform structure types from the readable owner declaration frontend. Owner-local shaders SHALL be able to declare cbuffers explicitly using those types and select registers in shader source without repeating individual fields. Common flat targets and existing instance-array protocols SHALL retain compatible declarations where required. This path SHALL use the existing generated virtual-include mechanism without a shader annotation parser or C++ header-processing tool. Owned declaration formatting SHALL be idempotent and SHALL preserve following top-level shader indentation.

#### Scenario: Explicit Contact cbuffer
- **WHEN** ContactShadows includes ContactShadowParameters.generated.hlsli and declares ContactV1 with FContactV1Uniform ContactShadow at b0
- **THEN** ContactShadow.Viewport and the other generated fields are available, and native reflection supplies the actual register and layout for validation

#### Scenario: Formatting readable declarations
- **WHEN** the formatter runs repeatedly over uniform BEGIN/END declarations and shaders that consume generated types
- **THEN** the files remain unchanged after the first formatting pass and following top-level resources and functions retain their correct indentation

#### Scenario: Derived declaration changes
- **WHEN** a field type, order, packing convention or compatibility constraint changes generated include bytes or contract metadata
- **THEN** shader and prepared-program cache identity prevents incompatible reuse, with representation versions updated when required
