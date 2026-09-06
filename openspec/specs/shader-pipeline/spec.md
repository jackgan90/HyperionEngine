# shader-pipeline Specification

## Purpose
TBD - created by archiving change add-shader-compilation-pipeline. Update Purpose after archive.
## Requirements
### Requirement: Portable shader artifacts
The engine SHALL compile HLSL vertex and pixel shaders to DXIL and SPIR-V and generate MSL source through an internal wrapper.
#### Scenario: Valid shader
- **WHEN** the triangle shader is compiled for each target
- **THEN** DXIL and SPIR-V artifacts are nonempty and SPIR-V can be reflected and converted to MSL.

### Requirement: Correct cache invalidation
The compiler SHALL key cached artifacts by the toolchain, compilation options and source-root contents and reject corrupted cache bytes.
#### Scenario: Include modification
- **WHEN** an included file changes after an initial compilation
- **THEN** the next artifact has a different cache key and is compiled again.

### Requirement: Actionable diagnostics
The compiler SHALL propagate compilation diagnostics to engine callers.
#### Scenario: Invalid source
- **WHEN** invalid HLSL is compiled
- **THEN** the operation fails with a diagnostic containing the shader error.
