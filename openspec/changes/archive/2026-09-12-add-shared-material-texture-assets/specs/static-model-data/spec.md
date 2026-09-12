## MODIFIED Requirements

### Requirement: Reflected persistence
The engine SHALL register all persistent model data without serializing runtime GPU handles. Current native models SHALL store geometry, hierarchy and typed material asset references; material values and image pixels SHALL be independently stored assets. Embedded legacy model records SHALL have an explicit offline split-upgrade route.

#### Scenario: Reflected persistence acceptance
- **WHEN** a static model and its dependency graph are archived and restored
- **THEN** geometry, material slot references, independent materials, textures and hierarchy are preserved

#### Scenario: Legacy embedded upgrade
- **WHEN** AssetTool upgrades an embedded legacy model
- **THEN** it publishes a complete model/material/texture graph with equivalent supported appearance and preserves the old root on failure
