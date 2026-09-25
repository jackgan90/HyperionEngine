## MODIFIED Requirements

### Requirement: Default local application attachment
Editor SHALL provide plugin-owned current-user local discovery and attachment by default, with explicit startup disablement. Requests SHALL execute against services in the selected running application at Main update boundaries, including while non-drawable. Listener failure SHALL not destroy unrelated GUI/rendering features.

#### Scenario: Attach to an open scene
- **WHEN** an agent selects a running Editor instance and queries its scene
- **THEN** the returned identity, nodes and values belong to the scene displayed by that Editor

#### Scenario: Multiple applications
- **WHEN** two instances are running and a call specifies one connection
- **THEN** only that instance receives the request

### Requirement: Truthful host capabilities
Discovery SHALL describe host-specific history/save support and unavailable providers. Attached operations SHALL not create independent asset drafts while claiming to edit GUI documents. Domain paths SHALL resolve on the target application.

#### Scenario: History provider unavailable
- **WHEN** an agent queries or invokes undo without a scene history provider
- **THEN** the capability is reported unavailable without a fabricated undo stack
