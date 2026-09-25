## MODIFIED Requirements

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
- **THEN** Editor clears deleted selection without choosing a replacement, and undo restores the recreated root selection

### Requirement: Existing host view and result capabilities
Automation SHALL expose existing camera/view, render/preview setting and capture tasks through typed host services. Temporary browsing state SHALL remain separate from saved scene settings. Results SHALL state their actual readiness/completion point and use bounded metadata for target-local artifacts.

#### Scenario: Capture completion
- **WHEN** a supported capture operation completes successfully
- **THEN** its result identifies the completed artifact and does not merely report that a request was queued

#### Scenario: Model preview and failed content
- **WHEN** an agent opens a native model in Editor
- **THEN** it can control the asset preview camera and read terminal producer errors
- **AND** screenshot admission depends on drawable output rather than content readiness, so failure/loading screens remain capturable

#### Scenario: Optional capture provider absent
- **WHEN** capture support is absent or disabled
- **THEN** discovery or invocation provides an actionable unavailable reason while unrelated scene and asset operations remain usable

### Requirement: Normal application close
Editor SHALL expose host-owned close decisions with explicit save/discard/cancel semantics. Request results SHALL state acceptance rather than claim process termination. Already queued replies SHALL receive bounded shutdown draining without allowing stalled clients to block exit indefinitely.

#### Scenario: Save before exit fails
- **WHEN** saving a dirty document for a requested exit fails
- **THEN** the application remains open and exposes the failure; successful saves otherwise follow normal host exit and persistence

#### Scenario: Cancel exit
- **WHEN** an agent cancels an accepted save-before-exit decision before shutdown begins
- **THEN** admitted disk saves may finish but no exit is triggered by their completion
