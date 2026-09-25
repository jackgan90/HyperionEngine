## MODIFIED Requirements

### Requirement: Independent scene runtime lifecycle
Renderer SHALL expose a Main-owned scene entity that maintains the logical node collection, asynchronous reflected native scene/model loading, per-frame lifecycle updates, render synchronization and structured status without constructing a plugin, GUI or window. Validated group, model-placeholder, camera and light nodes and their relationships SHALL be installed before waiting for model resources. Source-format parsing SHALL remain outside Renderer. References SHALL resolve through Assets. CPU Scene SHALL remain independent of Renderer/RHI. Existing FModel attachments SHALL have one owner through the scene bridge.

#### Scenario: Scene without an application plugin
- **WHEN** a client constructs a runtime scene, loads a native manifest and ticks it
- **THEN** its camera/light nodes become queryable and valid models become render-ready with shared resources and independently queryable failures without Editor involvement

#### Scenario: Loading-time removal
- **WHEN** a node or subtree is removed before its model preparation completes
- **THEN** completion cannot recreate any removed node or publish to a reused generation

### Requirement: Scene operations and shutdown
The entity SHALL expose typed node enumeration, generation-safe editing, hierarchy operations and idempotent Close. Close SHALL cancel requests and join admitted preparation tasks before releasing render attachments and clearing Render camera/light/selection metadata; it SHALL finish before session destruction. A subsequent Load SHALL use a new attachment epoch. Editor-specific selection of an inspected node, input gestures and demo animation policy SHALL remain outside the entity, while camera/light authored state and persistent default selections SHALL belong to Scene.

#### Scenario: Edit a ready scene
- **WHEN** a client adds, disables, moves or removes model, camera or light nodes and ticks the scene
- **THEN** published state and structured counts reflect the current collection while model ready/failed counts retain model-only meaning

#### Scenario: Close while loading
- **WHEN** the scene is closed during asynchronous loading and closed again
- **THEN** all admitted work is joined, no late publication occurs and session teardown remains safe

#### Scenario: Reload with the same Scene identity
- **WHEN** a runtime entity reloads after an earlier attachment was closed
- **THEN** new nodes and frames use a new attachment epoch, and old frame tokens or delayed results cannot target the new attachment

### Requirement: Versioned node scene format and migration
The native hyperion.scene record SHALL advance to version 4 with unified node records and stable-ID selections. Plain scene source version 2 SHALL represent the same node graph. Native versions 1 through 3 and plain source version 1 SHALL remain readable through explicit migration, preserving model references, transforms, visibility and material selections while creating deterministic compatible camera and light nodes. Nested legacy records SHALL execute their own migrations. Existing type/importer IDs SHALL remain stable and affected importer revisions SHALL invalidate old conversion caches.

#### Scenario: Legacy camera and lighting
- **WHEN** an old scene containing Eye/Target/Near/Far and no light records is imported or upgraded
- **THEN** its new camera preserves the old view with the established legacy FOV and focus distance, and real default light nodes preserve prior lighting

#### Scenario: Legacy ID collision
- **WHEN** an old model uses an ID proposed for a migration-generated camera or light
- **THEN** migration allocates a deterministic unused ID without overwriting the model and all default selections refer to the generated nodes

#### Scenario: Current empty selections
- **WHEN** a version 4 or source version 2 scene has no camera or light selection
- **THEN** loading does not inject legacy defaults and later saving preserves the absence

#### Scenario: Invalid or unordered graph
- **WHEN** a document has children before parents, or contains a cycle, duplicate ID, dangling reference, wrong-kind selection or conflicting payload/transform representation
- **THEN** valid unordered graphs load correctly and invalid documents fail before installing any nodes from the new document
