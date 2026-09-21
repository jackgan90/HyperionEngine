# native-asset-management Specification

## Purpose
Define reflection-based native asset persistence, checked identities and portable references, shared dependency loading, bounded caches and ordered revisions.
## Requirements
### Requirement: Native reflected asset persistence
Assets SHALL load and save registered types through one reflection-based native pipeline using engine IO. Native containers SHALL carry stable identity, root type/schema, content revision, integrity information and a reflected dependency table. Runtime loading SHALL reject source formats with an actionable import diagnostic.

#### Scenario: Model and scene persistence
- **WHEN** a model, scene or newly registered test asset is saved and loaded
- **THEN** its persistent values and checked type are restored without a type-specific loader

#### Scenario: Corrupt metadata or payload
- **WHEN** a native asset has invalid integrity, conflicting header/body type or dependency metadata
- **THEN** loading fails before publishing a CPU object

### Requirement: Portable asset identity and references
Authoring references SHALL carry stable identity, expected type and package or content-relative location hints and SHALL follow current asset contents. Content revision SHALL remain an integrity/cache value; authoring save/import SHALL not pin references. Registry resolution SHALL reject conflicting identities and validate the resolved type and ID. Mounted caches, graphs and writes SHALL use canonical package paths. Explicit low-level revision requests SHALL remain validated.

#### Scenario: Relocate a content graph
- **WHEN** a native scene and dependencies are moved and mounts are updated
- **THEN** references resolve independently of source directory or process working directory

#### Scenario: Shared dependency edit
- **WHEN** two saved scenes reference one model or material and that dependency changes at its stable identity
- **THEN** reloading both scenes obtains the new content without rewriting either scene

#### Scenario: Cross-root persistence
- **WHEN** a Game scene referring to Engine resources is saved and reloaded
- **THEN** its references retain virtual locations and checked identities without embedding local disk paths or fixed revisions

### Requirement: Generic dependency management
Assets SHALL discover dependencies through recursive reflection, share their loading and expose per-dependency errors separately from root CPU and GPU readiness. Cycles SHALL fail with a dependency path without deadlock.

#### Scenario: Shared and failed dependencies
- **WHEN** several instances reference the same model and another reference is missing or incorrectly typed
- **THEN** the valid model is shared and each failed reference has an actionable diagnostic

#### Scenario: Dependency cycle
- **WHEN** reflected references form a cycle
- **THEN** dependency resolution terminates with the cycle path even with one Worker

### Requirement: Bounded cache and ordered revisions
The asset service SHALL bound retained completed assets by count and byte weight, retain in-flight producer sharing, isolate consumer cancellation and prune finished work. Explicit invalidation and successful writes SHALL affect future requests while old consumers retain immutable snapshots. Same-path writes and following reads SHALL respect admission order.

#### Scenario: Concurrent save and reload
- **WHEN** a save is admitted before a load of the same path
- **THEN** that load observes the successful new revision or its write failure and old consumers retain the old snapshot

#### Scenario: Cache eviction
- **WHEN** completed assets exceed the configured retention budget
- **THEN** unused cache ownership is evicted without invalidating consumer snapshots or duplicating in-flight producers
