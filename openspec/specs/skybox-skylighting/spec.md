# skybox-skylighting Specification

## Purpose
Define native HDR sky preprocessing, consistent infinite sky imagery and diffuse/specular environment lighting, reusable environment resources and reference-scene acceptance.
## Requirements
### Requirement: Native HDR sky preprocessing
AssetTool SHALL import equirectangular HDR and EXR sources recovered from provenance-tracked recipes into native sky assets with linear radiance cubemap, cosine-convolved nine-coefficient RGB SH and separate GGX roughness-prefiltered cubemap dependencies. Original source files SHALL remain untracked local cache data. Source/bake changes SHALL invalidate import products. Runtime SHALL consume native assets without source-image decoding or environment baking.

#### Scenario: Reimport changed source
- **WHEN** source pixels or bake quality change
- **THEN** a complete new pinned sky dependency generation is published and the old generation remains readable

#### Scenario: Constant environment
- **WHEN** a constant linear radiance image is baked
- **THEN** every filtered roughness level retains that radiance and SH irradiance evaluates to pi times the input within documented numerical tolerance

### Requirement: Consistent sky and lighting
Builtin lighting SHALL evaluate diffuse SH and split-sum GGX specular IBL from the same sky generation and common yaw/intensity as the sky background, using a shared BRDF integration LUT. Forward, Deferred, clustered and transparent paths SHALL use the common implementation, without fixed ambient contribution in sky mode. Emissive and unlit behavior SHALL remain independent.

#### Scenario: Rotate and replace sky
- **WHEN** an environment is rotated or replaced
- **THEN** the background, diffuse directionality and reflections change together, without mixing dependency generations

#### Scenario: No directional light
- **WHEN** the scene selects only a sky environment with clustered lighting enabled
- **THEN** diffuse and specular IBL remain active and agree with Forward rendering

### Requirement: Depth-tested HDR sky
The renderer SHALL draw an infinite sky after opaque/compatibility passes and before transparency into the shared HDR target, loading existing depth and color, testing far depth and disabling depth writes. Standard/Reversed Z and sub-viewports SHALL work. Sky imagery SHALL receive the same final exposure/tonemap as the scene.

#### Scenario: Camera translation and transparent geometry
- **WHEN** the camera translates without rotating while opaque, masked and blended geometry overlaps the sky
- **THEN** the sky has no positional parallax, opaque coverage blocks it, and blended geometry composites over the correct background

### Requirement: Reusable environment resources
Environment preprocessing and BRDF evaluation SHALL expose engine-owned reusable contracts suitable for future probe inputs. Camera movement SHALL NOT rebake or reupload immutable environment textures. Retained frames SHALL keep their resource generations valid until existing GPU retirement permits release.

#### Scenario: Moving camera after warmup
- **WHEN** the camera moves continuously with an unchanged ready sky
- **THEN** environment texture upload counts remain unchanged while reflection directions update

### Requirement: Reference environment acceptance
Delivery SHALL provide at least three differently lit native sky assets in HyperionAssets with recoverable HDR/EXR provenance and recipes, Sponza configuration and documented CPU/GPU/persistence validation. The shared BRDF LUT SHALL be published as an engine-owned Content asset referenced across mounts. Limitations concerning visibility, local reflections and solar energy SHALL be explicit.

#### Scenario: Shipped environments
- **WHEN** each shipped sky asset is selected in Sponza
- **THEN** visual and lighting respond, native save/reload retains the selection, and D3D12 validation reports no errors
