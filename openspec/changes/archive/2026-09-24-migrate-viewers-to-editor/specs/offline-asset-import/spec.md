## MODIFIED Requirements

### Requirement: Independent source import
AssetImport SHALL register source adapters independently of runtime Assets. All source and external dependency bytes SHALL pass through engine IO; conversion SHALL run on Workers and produce validated engine CPU data before native serialization. Runtime Assets SHALL remain independent of source import adapters; Editor may expose import through the separate shared service.

#### Scenario: Import model or scene
- **WHEN** glTF/GLB is imported as a model or scene, or an existing scene JSON is imported
- **THEN** complete native output preserves supported geometry, materials, hierarchy, instance sharing, transforms, visibility and camera

### Requirement: Asset tooling and build integration
An engine command-line tool SHALL support import, inspect/validate and legacy upgrade, return useful nonzero failures, and generate content and fixtures through the same C++ pipeline. Catalog asset creation SHALL no longer be supported. Ordinary Editor builds SHALL consume published content without importing sample sources. Explicit tools SHALL publish engine content into Content and sample content into the mounted external repository; transient fixtures SHALL remain build outputs.

#### Scenario: Fresh build
- **WHEN** Editor is built without Game sources or published sample content
- **THEN** compilation succeeds and a requested unavailable sample reports an actionable content error

#### Scenario: Removed catalog command
- **WHEN** the former catalog command is requested
- **THEN** the tool reports an unsupported command and writes no Catalog asset
