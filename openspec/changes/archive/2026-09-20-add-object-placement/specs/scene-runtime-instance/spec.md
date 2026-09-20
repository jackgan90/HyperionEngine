## ADDED Requirements

### Requirement: Dynamic native model assets
SceneInstance SHALL allow native model references to be registered and asynchronously prepared after scene load or in an initially empty scene. It SHALL own the editable reference table independently of immutable loaded manifests, validate native reference identity, reuse compatible prepared resources and expose pending/ready/failure state. Nodes SHALL retain saveable asset identity. Close and replacement SHALL cancel and join admitted work.

#### Scenario: Add a previously unused Engine model
- **WHEN** a client registers an Engine Cube asset, waits for readiness, adds a model node and saves the document
- **THEN** reload restores the Cube through a valid native reference without modifying the source model or loaded manifest

#### Scenario: Repeated asset and unused cached asset
- **WHEN** multiple nodes use the same registered model and all of them are later removed
- **THEN** their geometry is shared and an unused cached registration is omitted from the saved scene

#### Scenario: Failed or cancelled preparation
- **WHEN** registration fails or the scene is replaced while preparation is pending
- **THEN** failure remains scoped and no node is created or late result installed into the replacement document
