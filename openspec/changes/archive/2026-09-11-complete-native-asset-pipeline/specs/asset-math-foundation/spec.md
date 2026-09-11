## MODIFIED Requirements

### Requirement: Owned math and mesh data
The engine SHALL expose vendor-independent vectors, matrices and static triangle primitive conversion. External glTF primitive conversion SHALL belong to AssetImport; Assets SHALL retain engine mesh/image value contracts without a cgltf dependency.

#### Scenario: Static fixture
- **WHEN** a valid glTF triangle primitive is converted and transformed
- **THEN** owned positions and indices match the fixture and the matrix follows documented column-vector semantics
