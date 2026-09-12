## MODIFIED Requirements

### Requirement: Renderer-independent logical scene
The Scene module SHALL own a unified logical node hierarchy independently of Renderer and RHI. Supported nodes SHALL include groups, models, perspective cameras, directional lights and environment lights. Handles SHALL include scene identity, slot and generation; persistent node IDs SHALL be nonempty and unique within the scene. Main mutations SHALL support typed add/remove/edit, model asset attachment, local/world transform operations, enabled state, model visibility and material override with monotonic revisions and owned incremental changes. World transforms and effective enabled state SHALL derive from parent relationships; mutable duplicate model, camera or light authority outside the scene SHALL NOT be required.

#### Scenario: Reuse and foreign handles
- **WHEN** an old or foreign handle is used after slot reuse
- **THEN** the current node remains unchanged, including when the reused slot has a different node kind

#### Scenario: Shared asset independent state
- **WHEN** two model nodes reference one immutable asset and one is moved or hidden
- **THEN** the other retains its state and both share immutable asset data

#### Scenario: Typed enumeration
- **WHEN** a client enumerates a scene containing model, camera, light and group nodes
- **THEN** all nodes appear with their stable identity, kind and parent relationships while model-only diagnostics count only models

### Requirement: Ordered rendering attachment
A Main rendering bridge SHALL publish complete effective model, camera, light and scene-selection state through the existing ordered Render client. One publication SHALL associate all affected node kinds and inherited changes with a scene identity, attachment epoch and monotonic publication serial. Related member changes and removal SHALL be frame-atomic. Render SHALL NOT borrow Main mutable objects. Accepted removals SHALL progress without presentation and preserve existing frame leases. Admission failure SHALL leave unacknowledged changes retryable; an unexpected Render publication failure SHALL be observable at scene level and prevent mixed-state frame building while preserving cleanup.

#### Scenario: Initial transform and removal
- **WHEN** a transformed model is attached and subsequently removed while a snapshot exists
- **THEN** no identity-transform frame is exposed and the old snapshot remains valid after removal

#### Scenario: Late asset result
- **WHEN** loading finishes after the logical instance was removed
- **THEN** it cannot attach an asset or register primitives for a reused slot

#### Scenario: Minimized scene
- **WHEN** scene mutations occur while presentation is paused
- **THEN** the bridge and Render control queue continue applying changes and cleanup

#### Scenario: Metadata-only publication
- **WHEN** only camera, light or scene-selection state changes in a scene with no primitives
- **THEN** the change receives a Render publication and a scene-level completion/error receipt

#### Scenario: Initial empty scene attachment
- **WHEN** a scene is first attached while empty or while its manifest is still loading
- **THEN** the first Flush admits a valid empty scene publication and frame input does not fabricate an uninitialized token

#### Scenario: Failed scene publication
- **WHEN** a Render publication fails after admission
- **THEN** a subsequent scene frame reports the publication failure rather than combining old geometry with new metadata, and Close still drains admitted work

### Requirement: Unchanged logical scenes avoid full bridge scans
The Main scene bridge SHALL process typed logical changes and affected editable material revisions incrementally. An unchanged scene without editable material selections SHALL avoid per-node and per-instance publication preparation. Camera/light-only changes SHALL NOT scan or republish unchanged models, and metadata-only edits SHALL NOT invalidate geometry preparation. Readiness and error caching SHALL invalidate on relevant scene and resource publications.

#### Scenario: Ready immutable scene
- **WHEN** an unchanged scene with frozen/inherited materials is flushed repeatedly
- **THEN** the bridge publishes no new work and does not rebuild per-model material-version lists or unchanged camera/light tables

#### Scenario: Shared editable material changes
- **WHEN** an editable material used by multiple models changes without a logical model update
- **THEN** all affected selections synchronize atomically with their current geometry and overrides

#### Scenario: Move only a camera
- **WHEN** one camera moves in a scene with many unchanged models
- **THEN** camera state is published without preparing or replacing each model's local render state

## ADDED Requirements

### Requirement: Validated scene hierarchy editing
Scene SHALL support parent changes with explicit KeepLocal and KeepWorld modes, subtree deletion and node deletion preserving child world transforms. It SHALL reject cycles, self-parenting, foreign/stale parents, invalid transforms and impossible inverse-dependent edits before partially changing state. Propagation and validation SHALL be iterative and independent of serialized parent order. Disabling a node SHALL disable descendants; model-only visibility SHALL NOT disable descendant cameras or lights. Removing nodes SHALL clear scene selections referencing them.

#### Scenario: Reparent a mixed subtree
- **WHEN** a subtree containing models, a camera and a light is reparented with KeepWorld
- **THEN** its world poses remain unchanged, its local transforms update appropriately, and all node kinds retain their identities and properties

#### Scenario: Keep children on removal
- **WHEN** a parent is removed using the keep-children operation
- **THEN** its children move to the removed parent's parent with preserved world transforms, or the entire edit fails if preservation is invalid

#### Scenario: Invalid hierarchy mutation
- **WHEN** a client attempts to create a cycle or preserve world under a noninvertible parent
- **THEN** node data, relationships, settings and pending revisions remain unchanged

#### Scenario: Model visibility versus subtree enabled state
- **WHEN** a model node with a camera child is hidden through model visibility
- **THEN** the model stops drawing while the camera remains eligible, whereas disabling the parent node disables both
