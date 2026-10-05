# owned-component-operation-registration Specification

## Purpose
Define feature-owned typed component operation registration, explicit exposure and owner lifecycle, and shared scene editing and persistence semantics.

## Requirements
### Requirement: Feature-owned typed component operation adapter

The engine SHALL provide a public typed automation adapter that a component's owning feature can call without editing the central built-in type list. CPU Scene/SceneEditing MUST remain independent of concrete automation, GUI and renderer plugins. The adapter MUST use the authoritative registered component and request/result reflection descriptors.

#### Scenario: External component discovered and invoked
- **WHEN** a feature registers a CPU component and its typed get/set/batch automation adapter at startup
- **THEN** discovery exposes the selected stable operations and invocation edits that component without a transport type branch or central concrete-type addition

#### Scenario: Unknown or inconsistent component type
- **WHEN** registration does not match an authoritative registered scene component descriptor
- **THEN** registration fails before exposing callable operations

### Requirement: Explicit exposure and owner lifecycle

Registration SHALL require an owner, explicitly select read/write exposure and run on Main before catalog Seal. Duplicate, late or invalid example registration MUST fail before publishing part of the requested operation family. Scoped owner withdrawal MUST remove callable closures before provider destruction. Missing providers SHALL retain controlled unavailable discovery/invocation.

#### Scenario: Read-only component adapter
- **WHEN** an owner enables reads without write exposure
- **THEN** get is discoverable and set/set_batch are absent

#### Scenario: Duplicate or late family
- **WHEN** a requested identity already exists or the catalog has been sealed
- **THEN** registration fails and catalog operation membership remains unchanged

#### Scenario: Provider absence and withdrawal
- **WHEN** the provider is absent at startup or its owner registrations have been withdrawn
- **THEN** invocation returns controlled unavailable or unknown-operation behavior without dereferencing a missing or destroyed provider

### Requirement: Shared edit and persistence semantics

Component writes SHALL reuse existing SceneEditing validation, document/revision/idle checks, field policy, atomic batch commits, ComponentChanges, history and persistence. Existing built-in operation IDs and schemas MUST remain unchanged.

#### Scenario: Atomic failure preserves the document
- **WHEN** an edit contains an invalid field/value, stale revision/handle or a failing member of a batch
- **THEN** no partial component values, history or document revision are committed

#### Scenario: GUI and automation equivalence
- **WHEN** GUI/domain and automation perform the same component edit
- **THEN** they produce equivalent document content, dirty/history behavior, change facts, undo/redo and persisted component values
