## ADDED Requirements

### Requirement: Task based capability coverage
The engine SHALL expose existing human content-authoring, asset, view and tool tasks through discoverable automation operations, with a maintained inventory mapping tasks to shared services, operation IDs, host availability and validation evidence. Presentation-only window arrangement and raw C++ methods SHALL NOT define task parity.

#### Scenario: Discover a supported task
- **WHEN** an agent searches for an existing authoring task and describes its operation
- **THEN** it receives input/output schemas, examples, effects, completion semantics and any actual provider unavailability without reading source or enumerating all schemas at startup

### Requirement: Shared scene authoring
Automation SHALL support the existing scene node, selection, hierarchy, component, settings, transform, asset-reference, placement, history and persistence tasks through the same scene document validation and transaction paths used by the GUI.

#### Scenario: Atomic edit and GUI undo
- **WHEN** an attached agent edits a valid set of scene objects at the current revision
- **THEN** the GUI observes the committed result and one shared undo transaction restores the prior values

#### Scenario: Invalid or stale candidate
- **WHEN** a request contains an invalid target, component value, hierarchy or stale revision
- **THEN** the entire requested edit is rejected without partial mutation or history changes

#### Scenario: Per-object components and prepared resources
- **WHEN** targets have different component instance IDs or different unedited field values
- **THEN** a typed batch preserves those values and commits one atomic history transaction
- **AND** default creation of model resource bindings is rejected before mutation; prepared placement remains the shared creation path

#### Scenario: Host deletion selection
- **WHEN** a selected object is deleted through GUI or automation
- **THEN** Editor clears selection and Viewer selects its first remaining model according to their existing host policies

### Requirement: Shared document and content transitions
Automation SHALL discover content and open, close or replace documents and content roots through shared host transition services, preserving explicit dirty decisions, busy checks, retirement, preference persistence and handle invalidation.

#### Scenario: Reject implicit data loss
- **WHEN** an agent requests replacement while a current document is dirty without an explicit applicable save or discard policy
- **THEN** the operation reports the conflict and retains current documents and content

#### Scenario: Successful root replacement
- **WHEN** an authorized root replacement succeeds
- **THEN** target participants retire old work, old handles become invalid and Editor preferences reflect the same root as a GUI replacement

### Requirement: Shared live asset workspace
Attached asset operations SHALL address the actual GUI workspace documents and their generation, history, loading, saving and interaction state. Standalone and GUI asset editing SHALL reuse the same field, reference and encoding business validation.

#### Scenario: Edit an open material
- **WHEN** an agent changes a valid material field or reference in an asset already open in Editor
- **THEN** the existing tab observes the same document change, preview update, dirty state and undo history without a duplicate draft

#### Scenario: Failed workspace entry
- **WHEN** a workspace asset fails to load
- **THEN** an agent can query its stable entry identity and error, activate or close it, and reopen it after repair without resetting unrelated documents

#### Scenario: Reference completion becomes stale
- **WHEN** an asynchronous reference edit completes after its expected document generation is replaced
- **THEN** it does not overwrite the newer state and reports a controlled outcome

### Requirement: Existing host view and result capabilities
Automation SHALL expose existing camera/view, render/preview setting and capture tasks through typed host services. Temporary browsing state SHALL remain separate from saved scene settings. Results SHALL state their actual readiness/completion point and use bounded metadata for target-local artifacts.

#### Scenario: Capture completion
- **WHEN** a supported capture operation completes successfully
- **THEN** its result identifies the completed artifact and does not merely report that a request was queued

#### Scenario: ModelViewer and failed content
- **WHEN** an agent attaches to ModelViewer
- **THEN** it can control the browsing camera and main directional light and read terminal producer errors
- **AND** screenshot admission depends on drawable output rather than content readiness, so failure/loading screens remain capturable

#### Scenario: Optional capture provider absent
- **WHEN** capture support is absent or disabled
- **THEN** discovery or invocation provides an actionable unavailable reason while unrelated scene and asset operations remain usable

### Requirement: Normal application close
Editor and Viewer SHALL expose host-owned close decisions with explicit save/discard/cancel semantics. Request results SHALL state acceptance rather than claim process termination. Already queued replies SHALL receive bounded shutdown draining without allowing stalled clients to block exit indefinitely.

#### Scenario: Save before exit fails
- **WHEN** saving a dirty document for a requested exit fails
- **THEN** the application remains open and exposes the failure; successful saves otherwise follow normal host exit and persistence

#### Scenario: Cancel exit
- **WHEN** an agent cancels an accepted save-before-exit decision before shutdown begins
- **THEN** admitted disk saves may finish but no exit is triggered by their completion

### Requirement: Existing import and publication workflows
Automation SHALL adapt the existing asset import and publication services with discoverable typed requests, owned asynchronous work, explicit paths and existing publication/conflict rules, without invoking test-only entry points.

#### Scenario: Import and reopen
- **WHEN** an agent imports a supported source and publication completes
- **THEN** the returned native assets can be discovered and reopened through the same asset services as human-produced assets

### Requirement: Parity validation and bounded architecture changes
Capability additions MUST preserve the existing transport, catalog, session, plugin lifecycle and strict wire contracts. Validation SHALL include discovery, invocation, malformed/stale/busy rejection, shared GUI state/history, saved reload and relevant absence/shutdown paths. Domain extraction SHALL NOT introduce private cross-plugin dependencies or a second attached workspace.

#### Scenario: Domain operation added
- **WHEN** a new task adapter is registered
- **THEN** CLI and MCP discover and invoke it through the unchanged bootstrap/transport path and its provider is drained before destruction
