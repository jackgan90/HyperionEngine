## MODIFIED Requirements

### Requirement: Asset tooling and build integration
An engine command-line tool SHALL support import, inspect/validate and legacy upgrade, return useful nonzero failures, and generate content and fixtures through the same C++ pipeline. Catalog asset creation SHALL no longer be supported. Ordinary Viewer builds SHALL consume published content without importing sample sources. Explicit tools SHALL publish engine content into Content and sample content into the mounted external repository; transient fixtures SHALL remain build outputs.

#### Scenario: Fresh build
- **WHEN** Viewer is built without Game sources or published sample content
- **THEN** compilation succeeds and a requested unavailable sample reports an actionable content error

#### Scenario: Removed catalog command
- **WHEN** the former catalog command is requested
- **THEN** the tool reports an unsupported command and writes no Catalog asset
