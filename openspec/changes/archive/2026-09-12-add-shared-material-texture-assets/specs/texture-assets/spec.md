## ADDED Requirements

### Requirement: Reflected texture assets
The engine SHALL store independent reflected texture assets with validated dimensions, RGBA8 pixels, complete mip chains and linear or sRGB encoding. Texture data SHALL remain independent of RHI. Samplers SHALL be material binding data.

#### Scenario: Texture round trip
- **WHEN** a texture asset is saved and loaded through generic Assets
- **THEN** its encoding and every mip byte are preserved and invalid sizes or mip layouts are rejected

### Requirement: Shared texture interpretation
Offline mip generation SHALL honor texture color interpretation. Import identity SHALL distinguish linear and sRGB interpretations and mip settings. Runtime preparation SHALL reuse one immutable texture source for the same loaded asset revision across models.

#### Scenario: Shared image with distinct bindings
- **WHEN** two independent models use one texture revision with different samplers
- **THEN** they share pixel and GPU texture storage while retaining independent sampler bindings

#### Scenario: Color role variants
- **WHEN** one source image is used as color and numeric data
- **THEN** its generated texture variants retain role-correct encoding and mip values
