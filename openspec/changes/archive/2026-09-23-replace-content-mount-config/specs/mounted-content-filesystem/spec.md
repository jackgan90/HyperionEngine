## MODIFIED Requirements

### Requirement: Application composition
Applications SHALL configure the Engine resource directory programmatically before engine asset/shader requests. Game SHALL start unmounted unless explicitly selected through the shared root service; no mount configuration file or sibling asset directory SHALL be required or automatically loaded. Optional directory arguments SHALL use that same service before dependent requests. Relative CLI directories SHALL resolve against the working directory. Runtime content browsing and persistence SHALL expose package paths. Explicit local file tools and read-only Engine authoring controls SHALL remain supported.

#### Scenario: Custom content location
- **WHEN** Viewer is given an explicit asset root and selects a sky
- **THEN** sky enumeration, loading and saved references use the configured virtual namespace without a mount configuration file

#### Scenario: No selected project
- **WHEN** Editor or automation starts without an explicit or restored asset root
- **THEN** Engine remains available, Game remains unmounted and the content root service remains usable

#### Scenario: Validate a mounted library subtree
- **WHEN** AssetTool validates a physical directory within a mount or its equivalent package path
- **THEN** it validates the assets in that requested subtree using complete mount indexes for dependency resolution
- **AND** corrupt payloads and nonexistent directories fail rather than being reported as an empty successful library
