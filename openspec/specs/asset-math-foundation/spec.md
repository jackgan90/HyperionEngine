# asset-math-foundation Specification

## Purpose
TBD - created by archiving change add-asset-and-math-foundation. Update Purpose after archive.
## Requirements
### Requirement: Owned math and mesh data
The engine SHALL expose vendor-independent vectors, matrices and static triangle primitive conversion. External glTF primitive conversion SHALL belong to AssetImport; Assets SHALL retain engine mesh/image value contracts without a cgltf dependency.

#### Scenario: Static fixture
- **WHEN** a valid glTF triangle primitive is converted and transformed
- **THEN** owned positions and indices match the fixture and the matrix follows documented column-vector semantics

### Requirement: Image round trips
The engine SHALL support PNG and linear EXR RGBA images through its image wrapper.
#### Scenario: Save and load
- **WHEN** a small known image is saved and reloaded
- **THEN** dimensions and channels match within PNG quantization or EXR float tolerance.

### Requirement: Persistent asset references
The engine SHALL persist asset identity and source path through reflection.
#### Scenario: Reference round trip
- **WHEN** an asset reference is saved and restored
- **THEN** its identity and path are preserved.
