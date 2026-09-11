## ADDED Requirements

### Requirement: Independent source import
AssetImport SHALL register source adapters independently of runtime Assets. All source and external dependency bytes SHALL pass through engine IO; conversion SHALL run on Workers and produce validated engine CPU data before native serialization. The Viewer SHALL not link AssetImport.

#### Scenario: Import model or scene
- **WHEN** glTF/GLB is imported as a model or scene, or an existing scene JSON is imported
- **THEN** complete native output preserves supported geometry, materials, hierarchy, instance sharing, transforms, visibility and camera

### Requirement: Reproducible incremental imports
Import records SHALL store stable output identities, importer version, settings and source/dependency fingerprints. Unchanged imports SHALL verify and reuse valid output; changed buffers/images/settings SHALL trigger rebuilding while retaining asset identity.

#### Scenario: External image changes
- **WHEN** only an external texture changes
- **THEN** reimport changes the content revision while keeping the asset ID and subsequent native loads see the changed pixels

### Requirement: Transactional content publication
Batch imports SHALL publish the root only after all dependencies are complete, preserve the previous graph on failure and keep existing readers on complete immutable revisions. Conflicting publication to one output SHALL be serialized or rejected.

#### Scenario: Failed scene reimport
- **WHEN** one dependency fails during reimport
- **THEN** the previously published scene and its dependencies remain loadable

### Requirement: Asset tooling and build integration
An engine command-line tool SHALL support import, inspect/validate, catalog creation and legacy upgrade, return useful nonzero failures, and generate sample content and fixtures through the same C++ pipeline. Generated assets SHALL live under the build/output directory.

#### Scenario: Fresh build
- **WHEN** Viewer is built from source assets without preexisting generated native content
- **THEN** required native model/scene/shadow samples are generated before running the configured Viewer
