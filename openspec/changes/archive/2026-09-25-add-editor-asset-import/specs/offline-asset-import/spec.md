## ADDED Requirements

### Requirement: Standalone image conversion
AssetImport SHALL convert PNG/JPG/JPEG into the existing RGBA8 Texture2D asset with the existing full mip algorithm, source tracking and native publication. Standalone images SHALL default to sRGB and accept explicit linear encoding. Existing JSON/native texture paths SHALL remain available.

#### Scenario: Import and reimport a linear image
- **WHEN** a PNG is imported with linear encoding and then repeated unchanged
- **THEN** the texture contains linear pixels and the full mip chain, keeps its identity and performs zero writes on the second import

### Requirement: Explicit sky conversion settings
HDR/EXR and sky recipe import SHALL accept optional radiance size, specular size and sample settings using the current bake algorithm and bounds. Explicit settings SHALL override recipe values; omitted settings SHALL preserve recipe/default behavior. Sources SHALL remain 2:1 HDR panoramas. Effective conversion changes SHALL invalidate incremental freshness without introducing temporary source recipes.

#### Scenario: Adjust sky bake settings
- **WHEN** the same HDR is reimported with a different valid bake size
- **THEN** the existing sky identity is preserved and the bake products reflect the new size

#### Scenario: Unchanged explicit settings
- **WHEN** sources and explicit settings are unchanged and the native graph is valid
- **THEN** reimport performs zero writes

#### Scenario: Invalid settings
- **WHEN** a non-power-of-two size or specular size exceeding radiance size is requested
- **THEN** import rejects the settings without publication
