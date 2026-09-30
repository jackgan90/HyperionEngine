## ADDED Requirements

### Requirement: Focus-aware scene-wide selection
Editor SHALL select all live logical scene nodes on a non-repeat Ctrl+A press while Viewport or Outliner has focus. Selection SHALL include collapsed, filtered, hidden and disabled nodes, preserve a live primary, and use stable scene order when no primary exists. Text input, popup/modal UI, focus loss, foreign windows, navigation, placement, gizmo and hierarchy gestures SHALL retain input priority. Selection SHALL not change authored revision, dirty state or history.

#### Scenario: Same all-selection in both panels
- **WHEN** Ctrl+A is pressed in either eligible panel while some nodes are folded, filtered or disabled
- **THEN** every logical node is selected once, the prior live primary is retained and authored state is unchanged

#### Scenario: Text input owns Ctrl+A
- **WHEN** the Outliner search field or a property text input owns the frame's keyboard input
- **THEN** Ctrl+A selects text without changing object selection

#### Scenario: Empty scene and conflicting input
- **WHEN** an eligible empty scene receives Ctrl+A, or a conflicting gesture receives Ctrl+A
- **THEN** the former has empty selection and the latter retains its existing object selection

### Requirement: Inclusive displayed-order Outliner ranges
Outliner SHALL use its current submitted row sequence for an inclusive Shift-click interval, including scrolled-offscreen rows and excluding folded descendants or search exclusions. Shift SHALL replace selection; Ctrl+Shift SHALL add the range without toggling already selected interval members. The endpoint SHALL be primary. Non-Shift clicks SHALL establish a fixed anchor for consecutive Shift clicks. Invalid anchors SHALL fall back to a displayed primary, then the endpoint.

#### Scenario: Reverse and consecutive ranges
- **WHEN** the user clicks a row then Shift-clicks rows above and below it in succession
- **THEN** each selection covers the corresponding inclusive interval from the original anchor and the clicked endpoint becomes primary

#### Scenario: Filtered additive range
- **WHEN** Ctrl+Shift clicks a search-result endpoint with selected objects outside the interval
- **THEN** the search-order interval is added once, outside selections remain, and the endpoint becomes primary

### Requirement: Shared atomic selection and lifecycle
GUI and Automation SHALL use shared SceneEditing validation and commit logic for new selection capabilities. scene.selection.select_all SHALL be typed, reflected and discoverable, validate document/revision/idle state, and return a bounded count/primary summary. Explicit selection SHALL accept more than 128 handles within existing transport budgets. Duplicate, stale and foreign handles SHALL fail atomically. Range state SHALL reset or safely resolve after filter/document/history/external selection changes. Existing operation IDs, selection order, history and persistence behavior SHALL remain compatible.

#### Scenario: Large selection and discovery
- **WHEN** an agent discovers, describes and invokes scene.selection.select_all on a scene with more than 128 nodes
- **THEN** the GUI shares the complete selection and the result reports its count and primary without encoding the whole list

#### Scenario: Invalid request or missing provider
- **WHEN** a selection request has stale document/revision/handles, duplicates, a busy document or no provider
- **THEN** it fails with the existing controlled error contract and preserves selection and authored state

### Requirement: Selection-aware click and drag arbitration
Shift selection SHALL integrate with existing Outliner press/release arbitration without prematurely replacing the selection. A gesture that becomes a hierarchy drag SHALL not also commit a range click. Selected-row drag SHALL retain the selected group and primary. Expansion arrows, ordinary/Ctrl clicks and filtered keyboard activation SHALL remain usable.

#### Scenario: Drag or cancel a Shift gesture
- **WHEN** a Shift press crosses the drag threshold or is cancelled by Escape, right mouse, focus loss or invalidated scene state
- **THEN** no range click is committed and the existing drag/cancellation contract controls scene mutation
