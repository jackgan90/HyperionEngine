## ADDED Requirements

### Requirement: General reflected material descriptions
The engine SHALL persist a material's typed parameter declarations, defaults and values, semantic bindings, render state and multiple passes using reflected records. Persistent values SHALL support numeric, structure, array, sampler and texture-reference values and SHALL reject runtime-only buffer/target resources.

#### Scenario: General value round trip
- **WHEN** a material with structured numeric parameters and texture references is saved and loaded
- **THEN** its complete type/value contract is restored and dependencies are discovered by the generic reflection visitor

### Requirement: Authored shader execution
Each material pass SHALL persist vertex and pixel shader paths, entries, defines, usage and instance fallback metadata. Runtime preparation SHALL use the existing shader compiler and reflection-based binding validation.

#### Scenario: Custom shader asset
- **WHEN** a material asset selects a compatible custom shader entry or define without a C++ change
- **THEN** rendering reflects that selection and incompatible bindings fail with useful diagnostics

### Requirement: Immutable shared material state
The runtime SHALL reuse definitions and immutable prepared state for the same loaded material revision. Local typed overrides SHALL remain isolated. Numeric-only edits SHALL not rebuild model geometry or upload unchanged textures.

#### Scenario: Local material edit
- **WHEN** one of two instances sharing a material changes a numeric override
- **THEN** only that instance changes visually while geometry and texture resource identities remain shared
