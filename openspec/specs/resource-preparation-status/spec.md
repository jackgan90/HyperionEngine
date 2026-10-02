# resource-preparation-status Specification

## Purpose
Define message-independent placement and sky preparation states, shared placement admission, and compatible reflected diagnostics while preserving editing and resource lifecycle contracts.

## Requirements
### Requirement: Message-independent placement preparation
Placement SHALL expose Ready, Pending or Failed state with an explicit preparation stage and independent error information. GUI admission, asynchronous automation polling and final commit SHALL consume the same shared preparation rule. Presentation and catalog messages SHALL NOT determine state.

#### Scenario: Pending presentation changes
- **WHEN** a required resource is pending and its display text changes
- **THEN** placement remains pending and no node, selection or history transaction is created

#### Scenario: Colliding or empty error
- **WHEN** a required preparation fails with an error starting with Preparing or with an empty message
- **THEN** automation reports load_failed and GUI blocks placement, independently of the message

#### Scenario: CPU data and GPU preparation
- **WHEN** model data has loaded but its required rendering resource or preview material is not ready, or an icon has not completed GPU preparation
- **THEN** placement remains pending with the appropriate stage

#### Scenario: Ready placement
- **WHEN** required resources are ready and document identity, revision and interaction admission remain valid
- **THEN** one node is committed and selected through the existing shared history transaction

#### Scenario: Source material fallback
- **WHEN** model geometry and preview material are ready while native source materials are pending
- **THEN** existing transient fallback preview behavior remains available without adding a new placement admission gate

### Requirement: Typed sky preparation and compatible diagnostics
Sky preparation SHALL publish explicit loading, uploading, ready and failed states. Status counts and consumers SHALL use these states rather than display prefixes or error-message presence. Reflected diagnostics SHALL retain the existing string state/error representation, schema, type ID and version, including empty non-applicable state and unrequested state.

#### Scenario: Wire and schema compatibility
- **WHEN** any supported sky state is encoded through reflection or queried through scene.lighting.get
- **THEN** state is the existing stable string, error is a separate string and the schema continues to describe string fields

#### Scenario: Sky failure presentation changes
- **WHEN** a sky preparation fails and its formatted message changes or its error text is empty
- **THEN** asset preview and light diagnostics still recognize failure and scene readiness does not report success

#### Scenario: Failed replacement and recovery
- **WHEN** a sky replacement fails and a valid sky is subsequently selected
- **THEN** existing retained-data and selection behavior is preserved and the valid preparation can reach ready

### Requirement: Existing task and editing contracts
The change SHALL preserve operation IDs, error codes, required input fields, task completion semantics, document/revision admission, Undo/Redo, explicit persistence, transient resource retention and task drain behavior. Runtime transports SHALL remain independent of placement and sky domains.

#### Scenario: Stale or cancelled placement
- **WHEN** document/revision changes during preparation or a GUI drag is cancelled
- **THEN** no partial placement or extra history transaction is committed

#### Scenario: Shutdown with pending resources
- **WHEN** scene replacement, content root replacement or shutdown retires pending preparation
- **THEN** captured state and GPU resources retain their existing drain and retirement guarantees
