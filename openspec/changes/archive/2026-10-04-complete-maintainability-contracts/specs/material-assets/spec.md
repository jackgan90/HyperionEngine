## MODIFIED Requirements

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
