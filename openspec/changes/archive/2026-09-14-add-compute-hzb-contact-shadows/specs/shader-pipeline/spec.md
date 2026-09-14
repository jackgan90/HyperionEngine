## MODIFIED Requirements

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
