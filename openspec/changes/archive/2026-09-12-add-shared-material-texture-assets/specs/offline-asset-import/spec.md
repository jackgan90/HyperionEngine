## ADDED Requirements

### Requirement: Named subasset conversion
Importers SHALL emit a root and named typed subassets without runtime format-specific loading. glTF/GLB import SHALL split materials and external, data-URI or buffer-view images into native assets. Stable names SHALL preserve editable asset identity across reimport.

#### Scenario: Embedded image import
- **WHEN** a GLB with embedded images and multiple material slots is imported
- **THEN** the root refers to native materials and textures and no original source file is needed for runtime loading

### Requirement: Shared content library
A configurable shared library SHALL map stable source/subasset/interpretation keys to native asset identities and immutable revisions across independently imported roots. Equal material values alone SHALL not merge distinct source materials. Library updates SHALL be serialized, dependency-complete and compatible with pinned revisions.

#### Scenario: Independent roots reuse a texture
- **WHEN** two models using the same canonical external image and interpretation are imported into one library
- **THEN** both graphs reference the same texture identity/revision without duplicating the image payload

#### Scenario: Failed shared reimport
- **WHEN** publication fails after preparing a new shared texture revision
- **THEN** every previously published root continues to load its prior pinned graph
