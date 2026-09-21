# offline-asset-import Specification

## Purpose
Convert external model and scene sources through independent engine-owned importers, track incremental provenance and publish complete native dependency graphs transactionally.
## Requirements
### Requirement: Independent source import
AssetImport SHALL register source adapters independently of runtime Assets. All source and external dependency bytes SHALL pass through engine IO; conversion SHALL run on Workers and produce validated engine CPU data before native serialization. The Viewer SHALL not link AssetImport.

#### Scenario: Import model or scene
- **WHEN** glTF/GLB is imported as a model or scene, or an existing scene JSON is imported
- **THEN** complete native output preserves supported geometry, materials, hierarchy, instance sharing, transforms, visibility and camera

### Requirement: Reproducible incremental imports
Import records SHALL optionally store portable logical source identities, stable output identities, importer version, settings and input fingerprints. Same-target reimport SHALL preserve the root and surviving child identities. Unchanged valid imports SHALL perform no writes. Neither root provenance nor generated dependency keys SHALL persist machine absolute paths. Different explicit new targets SHALL be allowed independent root identities.

#### Scenario: Source cache relocation
- **WHEN** a cloned native library is reimported from unchanged inputs at a different physical source/output location with the same settings
- **THEN** existing identities and content references remain stable and unchanged valid files are not rewritten

#### Scenario: Default import portability
- **WHEN** a user imports without configuring a source namespace
- **THEN** the resulting native graph contains no absolute local source path and same-target reimport can use a newly selected physical source location

### Requirement: Transactional content publication
Import SHALL finish conversion and source checks before modifying current files, serialize conflicting library publication, atomically replace each changed file, and restore previous files/remove newly created files on synchronous publication failure. Active consumers SHALL retain valid immutable CPU/GPU snapshots. Authoring publication SHALL use a quiescent reader boundary rather than promise cross-process multi-file atomicity.

#### Scenario: Previously shared products diverge
- **WHEN** multiple products in one import resolve to one native ID but require different contents
- **THEN** publication reports the conflicting identity before writing and preserves all previous files

#### Scenario: A reused texture changes in the same publication
- **WHEN** a texture has already been staged with new pixels and another product requests its former pixels
- **THEN** the old fingerprint does not reuse the staged texture with different content

#### Scenario: A native dependency is renamed
- **WHEN** a dependency retains its ID but moves within the discovered library or mounted roots
- **THEN** import freshness and external native validation resolve the current path by ID, and unchanged valid reimport performs zero writes

#### Scenario: Failed scene reimport
- **WHEN** conversion or dependency publication fails
- **THEN** previous roots and dependencies remain loadable after rollback and the failure is reported

### Requirement: Asset tooling and build integration
An engine command-line tool SHALL support import, inspect/validate, catalog creation and legacy upgrade, return useful nonzero failures, and generate content and fixtures through the same C++ pipeline. Ordinary Viewer builds SHALL consume published content without importing sample sources. Explicit tools SHALL publish engine content into Content and sample content into the mounted external repository; transient fixtures SHALL remain build outputs.

#### Scenario: Fresh build
- **WHEN** Viewer is built without Game sources or published sample content
- **THEN** compilation succeeds and a requested unavailable sample reports an actionable content error

### Requirement: Named subasset conversion
Importers SHALL emit a root and named typed subassets without runtime format-specific loading. glTF/GLB import SHALL split materials and external, data-URI or buffer-view images into native assets. Stable names SHALL preserve editable asset identity across reimport.

#### Scenario: Embedded image import
- **WHEN** a GLB with embedded images and multiple material slots is imported
- **THEN** the root refers to native materials and textures and no original source file is needed for runtime loading

### Requirement: Shared content library
A configured content directory SHALL retain one visible current file per native asset identity without requiring an authoritative library hasset. Discovery of portable per-root product mappings SHALL support stable reimport and shared source products, ignoring missing historical entries. Equivalent generated textures with matching interpretation SHALL be reusable; distinct editable source materials SHALL not merge merely because current parameter values match.

#### Scenario: Independent roots reuse a texture
- **WHEN** two models using one portable external image identity and interpretation are imported into the same content directory
- **THEN** both graphs reference one current texture file without revision-named duplicates

#### Scenario: Updated shared texture
- **WHEN** a shared texture changes during successful reimport
- **THEN** its ID and path remain stable and other roots obtain its current content when reloaded

### Requirement: Existing external native dependencies
Publication SHALL validate and preserve existing native dependencies across mounts without duplicating them. Current authoring references SHALL retain ID, type and package location without pinning the content digest. Failures SHALL preserve the prior publication.

#### Scenario: Shared engine LUT
- **WHEN** a Game sky is imported with the Engine BRDF dependency
- **THEN** it retains a current Engine package reference and no duplicate BRDF is published into Game
