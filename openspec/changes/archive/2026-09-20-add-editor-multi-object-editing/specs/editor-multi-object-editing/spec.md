## ADDED Requirements

### Requirement: Ordered shared selection
Editor SHALL maintain unique ordered selected handles and a primary equal to the most recently added remaining object. Plain clicks SHALL replace selection; Ctrl-clicks SHALL toggle membership. All selected Outliner rows and viewport representations SHALL highlight; only the primary SHALL own a gizmo. Selection SHALL NOT mutate document history or authored state.

#### Scenario: Toggle and remove primary
- **WHEN** A is selected, B is Ctrl-clicked, and B is Ctrl-clicked again in either selection surface
- **THEN** the intermediate selection is A,B with B primary, and the final selection is A with A primary

#### Scenario: Hidden rows and non-geometric objects
- **WHEN** filtering or collapsing hides selected rows, or a selected object lacks renderable geometry
- **THEN** selection is retained and visible selected non-geometric origins have a viewport selection marker

### Requirement: Common component inspection
Details SHALL show the intersection of present component types, compare each displayed property exactly and show Multiple Values for disagreement. Independent vector axes SHALL aggregate independently. Editing SHALL write only explicitly submitted fields to every target, including submissions equal to the primary value. Read-only properties SHALL remain read-only. Collections without reliable element correspondence SHALL be displayed as mixed and non-editable.

#### Scenario: Heterogeneous selection
- **WHEN** a model and point light are selected
- **THEN** only their common Transform component appears

#### Scenario: Mixed field and unchanged primary value
- **WHEN** two point lights have different intensities and the user submits the first light's current intensity
- **THEN** both intensities become that value in one history entry and their differing colors remain unchanged

#### Scenario: Unmatched sections
- **WHEN** selected models have incompatible asset or primitive section identities
- **THEN** section element edits are disabled without matching unrelated entries by index

### Requirement: Absolute Details transform editing
Details SHALL assign each edited local transform field absolutely to every selected object, including selected parents and children. Unedited axes and affine data SHALL remain intact. This SHALL be distinct from group gizmo manipulation.

#### Scenario: Assign mixed position axis
- **WHEN** selected local X positions are 10 and 20 and Details assigns X to 15
- **THEN** both local X positions become 15 while local Y and Z remain unchanged

### Requirement: Atomic batch history
Property and gizmo operations SHALL commit all target changes atomically and form one undo step per interaction. Invalid or stale targets SHALL leave the whole batch unchanged. Undo and redo SHALL restore all affected values together. Esc SHALL cancel gizmo previews without consuming the redo branch. Selection changes SHALL finish current editing before selecting new targets.

#### Scenario: One target rejects edit
- **WHEN** a candidate batch contains one invalid component or resource state
- **THEN** no target or document history changes

#### Scenario: Continuous edit undo redo
- **WHEN** a multi-object field is dragged through several values and undone then redone
- **THEN** one history item restores every original value then every final value

### Requirement: Selection-aware multi-subtree deletion
Delete SHALL remove the union of selected subtrees once, clear selection and record one command. Undo SHALL restore hierarchy, scene references and the original ordered selection and primary with current handle generations. Redo SHALL clear the selection and remove those subtrees again.

#### Scenario: Selected ancestor and descendant plus another root
- **WHEN** a parent, its child and an independent root are deleted and undone
- **THEN** each node is deleted/restored once, references and all three selections return, and one redo removes the same union
