## ADDED Requirements

### Requirement: Conditional property visibility
Record reflection SHALL allow a member to declare that it is visible only while a sibling inspected member equals one of a set of values. Descriptor validation SHALL reject conditions naming a missing, uninspected or self member. Inspection SHALL hide non-matching members in single-object editing, and SHALL hide them in multi-object editing when the controlling value is mixed or does not match. Hidden values SHALL remain stored, validated, persisted and available to automation.

#### Scenario: Switch environment source
- **WHEN** a Sky Light changes from SkyAsset to ConstantColor in Details
- **THEN** Sky asset, Tint, Yaw and Show background are hidden, Color is shown, and switching back restores the previously authored sky values

#### Scenario: Mixed controlling value
- **WHEN** two selected Sky Lights use different sources
- **THEN** the source is shown as mixed and members conditional on it are hidden

### Requirement: Typed asset reference selection
Reflected asset-reference members that declare a reference type SHALL be edited by a picker listing application-provided candidates of that type, instead of an optional override or nested identifier fields. The picker SHALL also accept a compatible Content Browser asset drop and show mixed values in multi-object editing. Selections SHALL submit validated transactions. Gui SHALL obtain candidates through an injected provider without depending on asset services.

#### Scenario: Choose another sky
- **WHEN** a user selects another sky asset from the Sky asset picker or drops one from the Content Browser
- **THEN** one undoable edit changes the requested reference, while incompatible assets are not offered or accepted
