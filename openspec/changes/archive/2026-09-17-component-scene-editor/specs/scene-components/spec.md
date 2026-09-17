## ADDED Requirements

### Requirement: Registered component composition
Scene objects SHALL own identified, registered CPU component values. Independent component types SHALL coexist, Transform SHALL be mandatory and unique, and registration SHALL define type identity, construction, reflection, validation, multiplicity and dependencies. Runtime Scene SHALL remain independent of Renderer, RHI and GUI.

#### Scenario: Compose capabilities
- **WHEN** an object has a camera and receives a point light
- **THEN** both components retain independent identity and state and both contribute to their consuming subsystems

#### Scenario: Add a custom component
- **WHEN** a module registers a reflected component using supported property kinds
- **THEN** it can be created, inspected and persisted without changing the Editor panel implementation

### Requirement: Transactional component state
All authoritative mutations SHALL occur on the owning Main thread through validated transactions. Invalid edits SHALL leave scene state, hierarchy, revision and publication unchanged. Stale object/component targets SHALL be rejected.

#### Scenario: Invalid camera edit
- **WHEN** a property transaction produces an invalid near/far pair
- **THEN** the transaction reports the validation error and preserves the complete prior component

#### Scenario: Edit after removal
- **WHEN** a queued edit targets a removed component or an obsolete scene generation
- **THEN** it cannot modify a replacement component or scene

### Requirement: Component persistence
Native scenes SHALL persist stable object/component/type identities and versioned authored state while excluding runtime resources. Legacy scenes SHALL migrate without losing authored meaning; unknown component types SHALL be preserved without execution or explicitly block unsafe saving.

#### Scenario: Round trip composition
- **WHEN** a composed object is saved and reopened
- **THEN** identities, hierarchy, component values and asset references are preserved
