# asset-math-foundation Specification

## Purpose
TBD - created by archiving change add-asset-and-math-foundation. Update Purpose after archive.
## Requirements
### Requirement: Owned math and mesh data
The engine SHALL expose vendor-independent vectors, matrices and static triangle primitive loading.
#### Scenario: Static fixture
- **WHEN** a valid glTF triangle primitive is loaded and transformed
- **THEN** owned positions and indices match the fixture and the matrix follows documented column-vector semantics.

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
