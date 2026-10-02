## ADDED Requirements

### Requirement: Owned registered component change facts

The existing Scene change stream SHALL expose owned registered component type and instance identities with Added, Modified and Removed occurrence flags. It SHALL compare live instances by both identities and descriptor equality independently of built-in component kinds and container positions. Empty slots SHALL NOT be reported as live instances. Latest node state SHALL remain distinct from accumulated facts, and deletion SHALL NOT require dereferencing removed storage. Opaque unknown archive envelopes SHALL retain existing preservation and metadata behavior without being interpreted as registered components.

#### Scenario: Repeated custom components
- **WHEN** a node contains two instances of a registered custom type and one instance changes
- **THEN** its type and instance identity is reported as modified while the unchanged instance is omitted

#### Scenario: Identity or type replacement
- **WHEN** a committed edit renames a component or replaces its type under the same instance ID
- **THEN** the old type and instance identity is reported removed and the new identity is reported added

#### Scenario: Removed node and slot reuse
- **WHEN** a node is removed and its slot is later reused
- **THEN** all of the removed node's live component identities remain observable under its original generation-bearing handle and do not merge with the new node

### Requirement: Component facts accumulate until acknowledgement

Scene SHALL merge component occurrence flags per type and instance identity across successful unacknowledged commits, including addition, modification and removal. It SHALL publish deterministic unique identity entries alongside the latest node state. Initial synchronization SHALL contribute additions for all current live registered components without losing pending facts or advancing the existing revision. Clear, subtree edits and shared-domain Undo/Redo SHALL use the same fact preparation. Facts SHALL NOT claim to be an ordered event history or to include uncommitted draft operations.

#### Scenario: Add then remove before acknowledgement
- **WHEN** a component is added and later removed in separate successful commits before acknowledgement
- **THEN** its accumulated entry contains both Added and Removed while the latest node state no longer contains the component

#### Scenario: Remove then readd and old acknowledgement
- **WHEN** an existing component is removed and readded before acknowledgement and the consumer acknowledges only an older revision
- **THEN** the newer change and its accumulated flags remain pending under the existing revision contract

#### Scenario: Shared-domain undo and redo
- **WHEN** a component mutation is executed, undone and redone through scene editing
- **THEN** each committed state change produces the same facts as the corresponding direct scene transaction

### Requirement: Atomic and selective component propagation

Component difference callbacks and fact allocations SHALL finish before authoritative mutation publication. Failure SHALL preserve node data, hierarchy, settings, pending changes, revision and derived query state. Existing built-in compatibility masks and effective transform/enabled behavior SHALL remain valid; unrelated custom metadata SHALL NOT invalidate model resources or spatial geometry. Scene SHALL remain independent of Renderer and RHI, and component registration alone SHALL NOT imply rendering or query behavior.

#### Scenario: Difference preparation fails
- **WHEN** a registered component equality callback throws during change preparation
- **THEN** the transaction fails without partial authoritative state, revision, pending fact or history changes

#### Scenario: Metadata and inherited transform
- **WHEN** an unrelated custom component is modified or only a parent's world transform affects a child
- **THEN** custom metadata does not rebuild unrelated model or query data, and the child receives the existing derived transform effect without a false local-component modification

#### Scenario: Built-in component identity changes only
- **WHEN** a unique built-in component is renamed without changing its semantic value
- **THEN** component identity facts are published while its existing resource and derived-state reuse behavior is preserved
