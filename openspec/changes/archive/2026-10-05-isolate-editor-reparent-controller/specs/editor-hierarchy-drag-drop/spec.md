## ADDED Requirements

### Requirement: Complete hierarchy gesture ownership
Editor SHALL keep hierarchy gesture snapshots, delivery alternatives, token generation and post-delivery expansion under one private controller. The plugin SHALL delegate without parallel mutable gesture/drop state. The controller MUST use the existing SceneEditing document, selection, validation and atomic reparent operation without retaining a second document/history or stored plugin callbacks.

#### Scenario: Gesture begins and completes
- **WHEN** an Outliner gesture crosses the existing threshold and delivers to a valid node or root
- **THEN** the controller owns its snapshot and named delivery state, commits through ReparentSceneNodes and clears the gesture before refreshing document interaction state

#### Scenario: Click or keyboard selection
- **WHEN** a row is activated by keyboard or a pointer gesture ends below the drag threshold
- **THEN** existing selection actions execute once with their original toggle/range semantics

### Requirement: Controller invalidation and retirement
The controller SHALL cancel on the existing policy/input/document/revision/selection invalidations. Document and content replacement and plugin shutdown MUST clear gestures, deliveries, expansions and matching GUI payloads before their providers retire. Cancellation/reset MUST be repeatable and safe during partial startup.

#### Scenario: Old document or selection
- **WHEN** the current document, revision or ordered selection no longer matches the gesture
- **THEN** continuation cancels without a domain mutation or history entry

#### Scenario: Document/content replacement or stop
- **WHEN** a document/content reset or plugin shutdown happens with a gesture or pending expansion
- **THEN** the controller releases its state and matching payload before document detach/GUI retirement, and repeating reset is safe

#### Scenario: Partial startup cleanup
- **WHEN** shutdown runs before acquiring a GUI provider
- **THEN** controller-owned state is cleared without dereferencing a missing provider

#### Scenario: Replacement load fails synchronously
- **WHEN** replacement passes save/dirty admission but path validation or loading throws after old scene retirement begins
- **THEN** the old gesture, delivery and matching GUI payload are already cleared, domain history is not prematurely reset and a subsequent valid scene can be opened
