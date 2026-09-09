## MODIFIED Requirements

### Requirement: Bounded executable resource capabilities
The device SHALL report enabled binding kinds, shader stages, register spaces, counts, constant alignment/range and descriptor capacities. Graphics support SHALL include fixed resource arrays, 2D RGBA8 sampled textures, sampled D32 depth targets, independent ordinary/comparison samplers and validated read-only structured/raw buffer views. VS texture sampling SHALL use a resource state valid for that stage. Unsupported mandatory features SHALL fail before submission.

#### Scenario: Read-only buffer view
- **WHEN** a shader reads a valid structured or raw buffer view
- **THEN** the view's usage, stride, extent and alignment are validated and the expected bytes reach the shader

#### Scenario: Vertex texture sampling
- **WHEN** a vertex shader samples an uploaded 2D texture
- **THEN** its descriptor is visible to VS and the uploaded resource is ready in a compatible shader-resource state

#### Scenario: Comparison sampler request
- **WHEN** a supported pipeline requires SamplerComparisonState for a sampled depth texture
- **THEN** reflection, layout and sampler comparison state are validated and execute the requested comparison

## ADDED Requirements

### Requirement: Renderer-produced texture sources
Engine-owned CPU resource descriptions SHALL identify renderer-produced sampled depth targets without exposing RHI/native objects to Scene or Materials. Renderer SHALL resolve the same identity for attachment writes and material reads and preserve descriptor identity across content updates.

#### Scenario: Reuse a rendered texture
- **WHEN** a depth source is rendered again with unchanged dimensions and binding layout
- **THEN** its texture and binding sets are reused while subsequent consumers see the new depth contents
