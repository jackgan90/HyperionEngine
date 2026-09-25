## MODIFIED Requirements

### Requirement: Default lighting is explicit scene content
Legacy scene migration SHALL create actual default light nodes matching established radiance and direction. An empty FScene SHALL remain empty. An explicit authored light edit SHALL mutate or explicitly create and select a scene light once after scene initialization, rather than continuously injecting session parameters.

#### Scenario: Load a legacy scene
- **WHEN** a legacy scene without stored lights is migrated
- **THEN** the migrated scene contains deterministic default light nodes, preserves the prior appearance, and subsequent edits to those nodes can be saved

#### Scenario: CLI override followed by save
- **WHEN** a user applies the existing light-direction CLI override and saves the scene
- **THEN** the saved scene contains the overridden scene-node direction and reload does not restore the prior session default
