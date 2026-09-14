## MODIFIED Requirements

### Requirement: Native HDR sky preprocessing
AssetTool SHALL import equirectangular HDR and EXR sources recovered from provenance-tracked recipes into native sky assets with linear radiance cubemap, cosine-convolved nine-coefficient RGB SH and separate GGX roughness-prefiltered cubemap dependencies. Original source files SHALL remain untracked local cache data. Source/bake changes SHALL invalidate import products. Runtime SHALL consume native assets without source-image decoding or environment baking.

#### Scenario: Reimport changed source
- **WHEN** source pixels or bake quality change
- **THEN** a complete new pinned sky dependency generation is published and the old generation remains readable

#### Scenario: Constant environment
- **WHEN** a constant linear radiance image is baked
- **THEN** every filtered roughness level retains that radiance and SH irradiance evaluates to pi times the input within documented numerical tolerance

### Requirement: Reference environment acceptance
Delivery SHALL provide at least three differently lit native sky assets in HyperionAssets with recoverable HDR/EXR provenance and recipes, Sponza configuration and documented CPU/GPU/persistence validation. The shared BRDF LUT SHALL be published as an engine-owned Content asset referenced across mounts. Limitations concerning visibility, local reflections and solar energy SHALL be explicit.

#### Scenario: Shipped environments
- **WHEN** each shipped sky asset is selected in Sponza
- **THEN** visual and lighting respond, native save/reload retains the selection, and D3D12 validation reports no errors
