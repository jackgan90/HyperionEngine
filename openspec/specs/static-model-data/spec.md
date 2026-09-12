# static-model-data Specification

## Purpose
Represent and validate static model geometry, materials and node instances as reflected CPU data independent of rendering backends.
## Requirements
### Requirement: Owned static scene model
The engine SHALL represent multiple primitives, materials and transformed node instances using engine-owned CPU types.

#### Scenario: Owned static scene model acceptance
- **WHEN** two nodes instance a shared geometry under parent transforms
- **THEN** both instances have correct world transforms while geometry remains shared

### Requirement: Reflected persistence
The engine SHALL register all persistent model data without serializing runtime GPU handles. Current native models SHALL store geometry, hierarchy and typed material asset references; material values and image pixels SHALL be independently stored assets. Embedded legacy model records SHALL have an explicit offline split-upgrade route.

#### Scenario: Reflected persistence acceptance
- **WHEN** a static model and its dependency graph are archived and restored
- **THEN** geometry, material slot references, independent materials, textures and hierarchy are preserved

#### Scenario: Legacy embedded upgrade
- **WHEN** AssetTool upgrades an embedded legacy model
- **THEN** it publishes a complete model/material/texture graph with equivalent supported appearance and preserves the old root on failure

### Requirement: Validated graph
The engine SHALL reject invalid indices, cycles and non-finite model data.

#### Scenario: Validated graph acceptance
- **WHEN** a node refers to an invalid child or the hierarchy cycles
- **THEN** model validation fails with a diagnostic
