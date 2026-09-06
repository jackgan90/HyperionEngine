## ADDED Requirements

### Requirement: Owned static scene model
The engine SHALL represent multiple primitives, materials and transformed node instances using engine-owned CPU types.

#### Scenario: Owned static scene model acceptance
- **WHEN** two nodes instance a shared geometry under parent transforms
- **THEN** both instances have correct world transforms while geometry remains shared

### Requirement: Reflected persistence
The engine SHALL register all persistent model data without serializing runtime GPU handles.

#### Scenario: Reflected persistence acceptance
- **WHEN** a static model is archived and restored
- **THEN** geometry, materials, textures and hierarchy are preserved

### Requirement: Validated graph
The engine SHALL reject invalid indices, cycles and non-finite model data.

#### Scenario: Validated graph acceptance
- **WHEN** a node refers to an invalid child or the hierarchy cycles
- **THEN** model validation fails with a diagnostic
