## ADDED Requirements

### Requirement: Native sky asset controls
SceneViewer SHALL provide selectable shipped sky .hasset assets and an arbitrary native asset path with an explicit apply operation through engine GUI interfaces. It SHALL expose environment source, common intensity/yaw, sky visibility and requested/active loading/error state. All persistent changes SHALL use scene APIs and survive scene save/reload.

#### Scenario: Select another sky
- **WHEN** the user selects and applies a different sky hasset
- **THEN** the viewer remains responsive, reports loading or failure, and a successful replacement updates sky imagery and lighting together

#### Scenario: Invalid asset path
- **WHEN** the user applies a missing or wrong-type hasset
- **THEN** an actionable error is displayed without losing the requested path or existing scene edits
