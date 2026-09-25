## ADDED Requirements

### Requirement: Self-contained default sky
Engine SHALL provide Cloudy at `/Engine/Skies/Cloudy.hasset` with all runtime dependencies in Engine. Migration SHALL preserve IDs and baked values, remove the Game copies, update existing references and rebuilding recipes, and retain source attribution.

#### Scenario: Engine-only loading
- **WHEN** no Game root is configured
- **THEN** the default sky graph loads successfully without external content

#### Scenario: Migrated scenes
- **WHEN** Sponza or another asset formerly referencing Cloudy loads or is rebuilt
- **THEN** it references the built-in sky with the preserved identity and no duplicate Game Cloudy is produced

#### Scenario: Reconstruct missing or damaged native sky
- **WHEN** the default sky native file is missing or damaged and the source recipe is rebuilt
- **THEN** its canonical root ID is retained and existing scene references resolve
- **AND** a request that conflicts with an existing valid output or another library asset is rejected before publication

#### Scenario: Clean source fixtures
- **WHEN** source-import test fixtures are prepared into an empty output directory
- **THEN** the Engine Cloudy HDR and Game source inputs are both present and hash-validated, or missing source caches produce an explicit failure

### Requirement: Shared default sky scene action
Editor SHALL expose Use Default Sky and automation SHALL expose `scene.sky.use_default` using the shared document/revision validation and history. The operation SHALL update the active environment or create and activate one when absent, enable sky rendering, preserve existing orientation/intensity, and support undo/redo and save/reload.

#### Scenario: Empty scene
- **WHEN** the user invokes the action without an active environment
- **THEN** one environment node and its active setting are created as one undoable transaction

#### Scenario: Existing environment
- **WHEN** the action is invoked with an active environment
- **THEN** its sky reference is replaced without adding a duplicate node, and Undo restores the prior component

#### Scenario: Stale request
- **WHEN** automation submits an outdated revision
- **THEN** the operation fails without changing the document or history

### Requirement: Default environment in model and material previews
Model and Material Asset Editor previews SHALL use the built-in sky for sky rendering and environment lighting, independently of Game selection and the main scene. Preview settings SHALL remain transient, and missing sky resources SHALL report diagnostics through existing readiness contracts.

#### Scenario: Preview isolation
- **WHEN** a model or material is opened and its preview is rendered
- **THEN** the default sky is present while the asset and main scene remain unchanged
