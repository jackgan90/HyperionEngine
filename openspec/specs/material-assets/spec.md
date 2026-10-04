# material-assets Specification

## Purpose
Define independent reflected material assets with authored shader passes, typed persistent parameters and texture references, supporting immutable sharing and isolated local edits.
## Requirements
### Requirement: General reflected material descriptions
The engine SHALL persist a material's typed parameter declarations, defaults and values, semantic bindings, render state and multiple passes using reflected records. Persistent values SHALL support numeric, structure, array, sampler and texture-reference values and SHALL reject runtime-only buffer/target resources.

#### Scenario: General value round trip
- **WHEN** a material with structured numeric parameters and texture references is saved and loaded
- **THEN** its complete type/value contract is restored and dependencies are discovered by the generic reflection visitor

### Requirement: Authored shader execution
Each material pass SHALL persist vertex and pixel shader paths, entries, defines, usage and instance fallback metadata. Runtime preparation SHALL use the existing shader compiler and reflection-based binding validation. Passes SHALL support an explicit silhouette fallback policy. Version 1 passes SHALL migrate using their historical fallback eligibility; version 2 SHALL persist the declared policy. Older wire inputs omitting the optional policy SHALL retain their historical meaning at the decode boundary. New explicitly authored passes SHALL be able to disable fallback independently of shader path.

#### Scenario: Custom shader asset
- **WHEN** a material asset selects a compatible custom shader entry or define without a C++ change
- **THEN** rendering reflects that selection and incompatible bindings fail with useful diagnostics

#### Scenario: Legacy material pass
- **WHEN** a fixed version 1 pass with a previously eligible or unsupported shader is loaded
- **THEN** migration preserves its old outline coverage decision and subsequent persistence records an explicit policy

#### Scenario: Policy-only material edit
- **WHEN** a material revision changes only its silhouette policy
- **THEN** definition publication and outline preparation observe the new policy without retaining a stale fallback decision

#### Scenario: Invalid silhouette declaration
- **WHEN** a vertex-only pass declares ModelShader fallback without a pixel shader
- **THEN** material definition and persistent asset validation reject the declaration before a selected rendering frame; a Disabled vertex-only pass remains valid

### Requirement: Immutable shared material state
The runtime SHALL reuse definitions and immutable prepared state for the same loaded material revision. Local typed overrides SHALL remain isolated. Numeric-only edits SHALL not rebuild model geometry or upload unchanged textures.

#### Scenario: Local material edit
- **WHEN** one of two instances sharing a material changes a numeric override
- **THEN** only that instance changes visually while geometry and texture resource identities remain shared
