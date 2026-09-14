## MODIFIED Requirements

### Requirement: Reproducible incremental imports
Import records SHALL store stable output identities, importer version, settings and source/dependency fingerprints. Unchanged imports SHALL verify and reuse valid output; changed buffers/images/settings SHALL trigger rebuilding while retaining asset identity. Configured source roots SHALL use machine-independent logical keys in publication identity/provenance.

#### Scenario: External image changes
- **WHEN** only an external texture changes
- **THEN** reimport changes the content revision while keeping the asset ID and subsequent native loads see the changed pixels

#### Scenario: Source cache relocation
- **WHEN** identical sources are reconstructed under another configured cache directory
- **THEN** published identity mappings remain usable and no local absolute source path is required

### Requirement: Asset tooling and build integration
An engine command-line tool SHALL support import, inspect/validate, catalog creation and legacy upgrade, return useful nonzero failures, and generate content and fixtures through the same C++ pipeline. Ordinary Viewer builds SHALL consume published content without importing sample sources. Explicit tools SHALL publish engine content into Content and sample content into the mounted external repository; transient fixtures SHALL remain build outputs.

#### Scenario: Fresh build
- **WHEN** Viewer is built without Game sources or published sample content
- **THEN** compilation succeeds and a requested unavailable sample reports an actionable content error

## ADDED Requirements

### Requirement: Existing external native dependencies
Publication SHALL validate and preserve references to existing native assets in other mounts rather than republishing those assets into the destination library. ID/type/revision validation SHALL apply before reuse and failures SHALL preserve the prior published root.

#### Scenario: Shared engine LUT
- **WHEN** a Game sky is imported with the Engine BRDF dependency
- **THEN** it retains an Engine package reference and no duplicate BRDF is published into Game
