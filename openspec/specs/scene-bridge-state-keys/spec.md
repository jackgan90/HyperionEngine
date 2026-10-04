# scene-bridge-state-keys Specification

## Purpose
Define named SceneBridge status and material dependency records while preserving publication, invalidation and resource lifetime behavior.
## Requirements
### Requirement: Named bridge status revision components

Renderer SHALL expose bridge status snapshots with explicitly named bridge-status and resource-publication components. Both general and model-only status queries SHALL preserve their existing counters, initial values, increment conditions and Main-domain ownership. Equality SHALL observe both components. SceneInstance SHALL retain its model-only status cache behavior when consuming the named snapshot.

#### Scenario: Idle and metadata-only updates
- **WHEN** a ready scene is flushed without changes, or camera/light metadata changes without affecting models
- **THEN** idle snapshots remain equal and metadata-only publications do not advance the model bridge-status component or cause a model-status rescan

#### Scenario: Model publication and independent resource completion
- **WHEN** model publication advances its bridge counter or render-resource completion advances the resource publication revision
- **THEN** the corresponding named component changes and status equality observes that change without exchanging component meanings

### Requirement: Named material selection and editable dependency records

SceneBridge SHALL represent model-surface snapshot revisions, section indices with associated snapshot revisions, and editable-instance dependencies using distinct named records. Equality SHALL preserve the existing identity/revision comparisons, absent-selection values and ordered section distinctions. Scene change masks SHALL continue to account for authored selections, overrides, source data, transforms and enabled state.

#### Scenario: Material-only update without a Scene edit
- **WHEN** a selected editable material publishes a new revision while the logical Scene has no changes
- **THEN** affected models are prepared with the new immutable snapshot, shared consumers use the same frozen snapshot, and subsequent unchanged flushes perform no additional preparation

#### Scenario: Change or remove a section selection
- **WHEN** a material selection is assigned to another section or removed
- **THEN** each section keeps the existing selection precedence and subsequent edits to a detached material do not prepare the former consumer

### Requirement: Equivalent publication and lifetime behavior

The named records SHALL preserve batch preparation before publication admission, commit timing, task receipt/error handling, shared reference lifetimes and GPU retirement. The change SHALL preserve reflected schemas, serialized content, operation IDs, rendering results and existing no-frame close/removal/reattachment behavior.

#### Scenario: Failed preparation remains atomic
- **WHEN** one model in a preparation batch has invalid material overrides
- **THEN** no related publication or cached material dependency is partially committed, and a corrected retry follows the existing successful path

#### Scenario: Pending work and retained resources
- **WHEN** attachments are removed or the bridge closes with queued work or retained render frames
- **THEN** existing task completion and resource retirement rules remain in effect without borrowing mutable Main state across execution domains
