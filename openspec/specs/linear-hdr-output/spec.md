# linear-hdr-output Specification

## Purpose
Define shared linear HDR scene composition, tonemapping, sRGB presentation and explicit display overlay color contracts for Forward and Deferred rendering.
## Requirements
### Requirement: Shared linear scene shading and display output
Forward and Deferred scene shading and transparent blending SHALL operate in linear HDR. BaseColor/emissive texture RGB SHALL decode sRGB, numeric maps and alpha SHALL remain data, and both pipelines SHALL use one shared tonemapping and final sRGB encoding contract. Material shaders SHALL NOT independently tone-map HDR scene draws. The initial common tone operator SHALL preserve the existing Reinhard formula with default exposure one.

#### Scenario: HDR highlight and transparent overlap
- **WHEN** light or emissive values exceed one and transparent objects overlap them
- **THEN** values remain HDR through composition and are tone-mapped once before presentation

#### Scenario: Shared pipeline comparison
- **WHEN** the same scene is rendered through Forward and Deferred with identical output settings
- **THEN** both use the same lighting and tone operator and differences are limited to documented storage/reconstruction tolerances

### Requirement: Explicit display overlays and clear colors
The renderer SHALL distinguish scene-linear values from authored display colors. GUI/debug overlays SHALL compose after scene tonemapping, with sRGB authoring values decoded before linear blending and exactly one encoding on output. Alpha/font coverage SHALL NOT receive gamma conversion. Empty-scene background SHALL retain its documented display-color interpretation.

#### Scenario: GUI over a bright scene
- **WHEN** GUI and a depth preview overlay an HDR scene
- **THEN** they are not tone-mapped as scene radiance and keep correct clipping, order and color interpretation
