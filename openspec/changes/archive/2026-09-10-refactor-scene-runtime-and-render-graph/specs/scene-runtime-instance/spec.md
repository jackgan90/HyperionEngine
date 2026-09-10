## ADDED Requirements

### Requirement: Independent scene runtime lifecycle

Renderer SHALL expose a Main-owned scene entity that maintains the model collection, asynchronous manifest/model loading, per-frame lifecycle updates, render synchronization and structured status without constructing a plugin, GUI or window. CPU Scene SHALL remain independent of Renderer/RHI. Existing FModel attachments SHALL have one owner through the scene bridge.

#### Scenario: Scene without a viewer plugin
- **WHEN** a client constructs a runtime scene, loads the current manifest and ticks it
- **THEN** its valid instances become render-ready with shared resources and independently queryable failures without SceneViewer involvement

#### Scenario: Loading-time removal
- **WHEN** an instance is removed before its model preparation completes
- **THEN** completion cannot recreate that instance or publish to a reused generation

### Requirement: Scene operations and shutdown

The entity SHALL expose model enumeration and generation-safe add/update/remove operations and idempotent Close. Close SHALL cancel requests and join admitted preparation tasks before releasing render attachments; it SHALL finish before session destruction. Viewer-specific selection, input, camera and demo animation policy SHALL remain outside this entity.

#### Scenario: Edit a ready scene
- **WHEN** a client adds, hides, moves or removes a model and ticks the scene
- **THEN** render state and ready/failed/model counts reflect the current collection

#### Scenario: Close while loading
- **WHEN** the scene is closed during asynchronous loading and closed again
- **THEN** all admitted work is joined, no late publication occurs and session teardown remains safe
