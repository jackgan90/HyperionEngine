# shader-pipeline Specification

## Purpose
TBD - created by archiving change add-shader-compilation-pipeline. Update Purpose after archive.
## Requirements
### Requirement: Portable shader artifacts
The engine SHALL compile HLSL vertex, pixel and compute shaders to DXIL and SPIR-V and generate MSL source through an internal wrapper. Each artifact SHALL expose normalized resource and member reflection for its actual compilation target or preserved intermediate source, including cbuffer type trees, sizes, offsets, array/matrix strides and major order, binding spaces/counts/stages and shader input/output signatures. Compute reflection SHALL include fixed thread-group dimensions and distinguish sampled resources from writable texture/structured/raw resources. Native compiler and reflection types SHALL remain private. MSL reflection SHALL retain the generated resource mapping without implying a Metal execution backend.

#### Scenario: Valid shader
- **WHEN** the triangle shader is compiled for each target
- **THEN** DXIL and SPIR-V artifacts are nonempty and SPIR-V can be reflected and converted to MSL, with normalized reflection available on all three artifact paths

#### Scenario: Complete member reflection
- **WHEN** a shader contains nested uniform structures, arrays, non-square matrices and resources in multiple spaces
- **THEN** callers can identify typed member paths and their actual target layout without inferring offsets from C++ structures or another target

#### Scenario: Binding conflicts and visibility
- **WHEN** vertex and pixel artifacts are combined into a material program
- **THEN** compatible bindings are merged, disjoint stage-specific declarations remain distinguishable, and overlapping incompatible declarations produce diagnostics

#### Scenario: Compute cache and reflection
- **WHEN** a compute shader with storage resources is compiled cold and from cache for each supported artifact target
- **THEN** stage, writable resource kinds, group dimensions, member layout and final bytes are preserved consistently

### Requirement: Correct cache invalidation
The compiler SHALL key cached artifacts by the toolchain, compilation options and source-root contents and reject corrupted cache bytes. Explicit defines SHALL be normalized with duplicate names rejected. Artifact/reflection schema and register-space mapping versions SHALL be part of cache identity. Cache hits SHALL reconstruct the same normalized reflection and final bytes as cold compilation, preserving the unstripped DXIL or SPIR-V intermediate required for reflection and MSL conversion.

#### Scenario: Include modification
- **WHEN** an included file changes after an initial compilation
- **THEN** the next artifact has a different cache key and is compiled again

#### Scenario: Cache hit with reflection
- **WHEN** a DXIL, SPIR-V or MSL shader is compiled again from a valid cache
- **THEN** its resource/member reflection and final bytes equal the cold result

#### Scenario: Option and mapping changes
- **WHEN** defines or the binding/reflection format version changes with unchanged source text
- **THEN** cache identity changes and stale interface data is not reused

### Requirement: Actionable diagnostics
The compiler SHALL propagate compilation diagnostics to engine callers.
#### Scenario: Invalid source
- **WHEN** invalid HLSL is compiled
- **THEN** the operation fails with a diagnostic containing the shader error.

### Requirement: Mounted text shader sources
Shaders SHALL remain plain text assets and compile through engine-owned filesystem access for main sources and includes. Engine and Game shader paths SHALL be supported, including Game includes of Engine shared files. Content relocation SHALL not change cache identity when logical source contents and compilation inputs are unchanged.

#### Scenario: External material shader
- **WHEN** a Game text shader includes an Engine shader header
- **THEN** supported targets compile and reflect through the same pipeline without local path references in the material

#### Scenario: Relocated warm cache
- **WHEN** mount physical locations change with identical logical contents
- **THEN** the same shader cache entry remains valid and changed includes still invalidate it
