# texture-assets Specification

## Purpose
Define independent CPU texture assets with validated color encoding and complete offline-generated mip chains, supporting shared image data and separate sampler bindings.
## Requirements
### Requirement: Reflected texture assets
The engine SHALL store independent reflected texture assets with validated dimensions, complete mip chains, RGBA8/RGBA16F/RGBA32F pixels and explicit 2D or Cube dimensions. Cube textures SHALL contain six square faces with identical formats and mip extents. Floating-point textures SHALL use linear encoding and finite values; RGBA8 SHALL support linear or sRGB encoding. Texture data SHALL remain independent of RHI. Samplers SHALL be material binding data. Existing records SHALL migrate to 2D RGBA8 without pixel changes.

#### Scenario: Texture round trip
- **WHEN** a texture asset is saved and loaded through generic Assets
- **THEN** its encoding, dimension, format and every face/mip byte are preserved and invalid sizes or layouts are rejected

#### Scenario: HDR cube upload
- **WHEN** a floating-point cubemap is sampled on D3D12
- **THEN** values above one and all six faces survive upload and typed cube binding rejects a 2D texture

### Requirement: Shared texture interpretation
Offline mip generation SHALL honor texture color interpretation. Import identity SHALL distinguish linear and sRGB interpretations and mip settings. Runtime preparation SHALL reuse one immutable texture source for the same loaded asset revision across models.

#### Scenario: Shared image with distinct bindings
- **WHEN** two independent models use one texture revision with different samplers
- **THEN** they share pixel and GPU texture storage while retaining independent sampler bindings

#### Scenario: Color role variants
- **WHEN** one source image is used as color and numeric data
- **THEN** its generated texture variants retain role-correct encoding and mip values
