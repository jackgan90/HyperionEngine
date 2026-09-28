## MODIFIED Requirements

### Requirement: Explicit main lighting selection
The scene SHALL persist explicit selections for one main directional light and one environment light. Multiple candidate nodes SHALL remain editable and persistable, but the builtin pipeline SHALL consume only the selected node of each kind. Empty selections and disabled selected nodes SHALL contribute zero corresponding radiance. Removing a selected node SHALL clear the selection without silently selecting a different light. Creating a node with an environment light, or adding an environment light component to an existing node, SHALL select it in the same undoable transaction only when no environment light is selected. Replacing a selected environment light SHALL require an explicit undoable settings change.

#### Scenario: Multiple candidate lights
- **WHEN** a scene contains two enabled directional-light candidates and selects only the second
- **THEN** builtin direct lighting and CSM use the second, and editing the first does not change the rendered lighting

#### Scenario: Remove the selected light
- **WHEN** the selected directional light is removed
- **THEN** subsequent frames have zero directional radiance and no directional shadows until the scene explicitly selects or creates a light

#### Scenario: Disable a light ancestor
- **WHEN** the selected light's ancestor is disabled and subsequently reenabled
- **THEN** that light contribution disappears and returns through normal scene publication without session parameter edits

#### Scenario: First and second sky light
- **WHEN** a sky light is created in a scene without an active environment and a second sky light is then created or added as a component
- **THEN** the first becomes active in its creation transaction, the second remains inactive, and Undo of the first creation clears the selection

#### Scenario: Explicit sky light activation
- **WHEN** the inactive sky light is set as the active sky light and the change is undone
- **THEN** the builtin pipeline switches to it and Undo restores the prior selection

### Requirement: Persistent environment source selection
Environment lights SHALL select either ConstantColor or SkyAsset, with an always-present native sky reference, a linear RGB tint, finite yaw in degrees, shared radiance intensity and independently controlled background visibility. New environment values SHALL default to SkyAsset with the Engine default sky. Sky orientation SHALL use explicit yaw, independent of node transform. SkyAsset mode SHALL replace constant ambient, and the constant color SHALL affect only ConstantColor mode. Existing environment records SHALL migrate while retaining their source, yaw orientation and requested sky. A record without a sky reference SHALL receive the Engine default sky. Snapshot and Save As SHALL preserve and rebase requested references, including pending or failed choices.

#### Scenario: Save a sky selection
- **WHEN** a sky reference, tint, yaw, intensity and visibility are edited and saved to another directory
- **THEN** loading the saved scene restores the same requested environment and correctly resolves its pinned dependencies

#### Scenario: Migrate an earlier environment record
- **WHEN** a version 2 environment record with yaw in radians and no sky reference is loaded
- **THEN** it keeps ConstantColor, its yaw is expressed in equivalent degrees, and its sky reference is the Engine default sky

#### Scenario: Reject an invalid sky reference
- **WHEN** an edit supplies an empty reference, a non-sky reference or a negative tint
- **THEN** the edit is rejected without changing the existing light
