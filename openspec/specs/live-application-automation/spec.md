# live-application-automation Specification

## Purpose
Define explicit attachment to running applications, authoritative live-scene operations, truthful host capabilities and connection lifetimes that preserve application-owned state.
## Requirements
### Requirement: Default local application attachment
Editor and Viewer SHALL provide plugin-owned current-user local discovery and attachment by default, with explicit startup disablement. Requests SHALL execute against services in the selected running application at Main update boundaries, including while non-drawable. Listener failure SHALL not destroy unrelated GUI/rendering features.

#### Scenario: Attach to an open scene
- **WHEN** an agent selects a running Editor instance and queries its scene
- **THEN** the returned identity, nodes and values belong to the scene displayed by that Editor

#### Scenario: Multiple applications
- **WHEN** two instances are running and a call specifies one connection
- **THEN** only that instance receives the request

### Requirement: Shared live scene mutations
Attached scene operations SHALL query bounded object data and edit transforms through the host's shared scene document service. Mutations SHALL validate document identity, object handles and expected revision, preserve unrelated fields and selection, and report actual Main commit state separately from rendering and saving.

#### Scenario: Transform and GUI undo
- **WHEN** an agent modifies an Editor object's transform and the user invokes normal Undo
- **THEN** the same shared history restores the previous transform and dirty state

#### Scenario: Concurrent interaction or stale handle
- **WHEN** a write conflicts with an active GUI transaction or addresses a replaced document or stale revision
- **THEN** it fails without interrupting the interaction or changing another object

### Requirement: Truthful host capabilities
Discovery SHALL describe host-specific history/save support and unavailable providers. Attached operations SHALL not create independent asset drafts while claiming to edit GUI documents. Domain paths SHALL resolve on the target application.

#### Scenario: Viewer history unavailable
- **WHEN** an agent queries or invokes undo in a Scene Viewer host without history
- **THEN** the capability is reported unavailable without a fabricated undo stack

### Requirement: Connection lifetime is independent of application lifetime
Disconnect SHALL stop session admission and safely drain admitted work without closing the application or discarding shared documents. Job and connection identities SHALL not be silently reused across targets or reconnects. Unknown mutation outcomes SHALL not trigger automatic retries.

#### Scenario: Client exits after editing
- **WHEN** a CLI or MCP client disconnects after committing a scene edit
- **THEN** the application remains open with its modified document and normal history
