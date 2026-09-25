## MODIFIED Requirements

### Requirement: Traditional default Deferred pipeline
Renderer SHALL provide a default raster Deferred pipeline with opaque/masked GBuffer BasePass followed by a vertex/pixel fullscreen LightingPass producing HDR scene color. It SHALL share CSM visibility, material evaluation and lighting formulas with selectable Forward. Compute lighting and mobile single-pass deferred SHALL NOT be required.

#### Scenario: Default scene frame
- **WHEN** Editor runs without an explicit pipeline override
- **THEN** BasePass writes GBuffer/depth, Lighting reads them and CSM, and shared output presents the scene with zero validation errors

#### Scenario: Empty or masked scene
- **WHEN** no valid opaque pixels remain or a masked pixel is clipped
- **THEN** cleared GBuffer validity and depth prevent undefined shading and preserve the background or visible geometry behind the mask
