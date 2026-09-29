## MODIFIED Requirements

### Requirement: Startup depth configuration
Editor SHALL default reversedZ to true, initialize it from saved settings, and apply validated changes through its shared GUI/automation settings service to the next rendered frame of scene and three-dimensional asset viewports. Saving SHALL remain an explicit independent operation. Changes SHALL NOT mutate scene or asset history.

#### Scenario: Missing setting
- **WHEN** an existing configuration omits reversedZ
- **THEN** Editor and its scene/asset viewports use reversed-Z

#### Scenario: Standard fallback and live edits
- **WHEN** reversedZ is false at startup and later edited to true
- **THEN** the next rendered scene and 3D asset preview frames use reversed-Z without restarting, reopening documents or recreating the swapchain
- **AND** saving persists true for the next launch

#### Scenario: Hidden viewport resumes
- **WHEN** depth changes while a viewport is hidden or minimized or before an asset preview is opened
- **THEN** its next rendered frame uses the latest committed depth convention

## ADDED Requirements

### Requirement: Live depth consistency
Editor SHALL use one committed convention for scene camera projections, depth targets and comparisons, shadows, HZB/contact shadows, transparency, sky, picking, placement and debug drawing. Frozen culling SHALL retain its physical frustum and frozen state across a depth change. Old GPU resources SHALL remain valid until in-flight use completes.

#### Scenario: Alternating frames
- **WHEN** the same scene session and swapchain render repeated Standard/Reversed switches
- **THEN** Forward and Deferred preserve visible geometry, shading and compositing within rendering tolerance, with no validation errors or unbounded retained resource growth

#### Scenario: Frozen culling and interactions
- **WHEN** a user freezes culling, moves the camera and changes depth convention
- **THEN** the frozen frustum retains its original physical coverage and picking/placement use the current camera with the committed convention

### Requirement: Live depth settings contract
The render.settings.get and render.settings.set operations SHALL expose version 2 metadata and preserve existing IDs and field shapes. activeReversedZ SHALL report the convention committed for subsequent frames. Set completion SHALL mean Main settings committed, not GPU presentation. Save and persistent record format SHALL remain version 1. GUI and automation SHALL share validation, revision and persistence behavior.

#### Scenario: Discovery and mutation
- **WHEN** a client describes and invokes render.settings.set with the current revision and valid values
- **THEN** the description explains next-frame application, the response and subsequent get report matching requested/active values and a new revision, and scene/asset history remains unchanged

#### Scenario: Rejected candidate
- **WHEN** a client submits invalid values or a stale revision
- **THEN** settings, active depth, frozen culling, saved files and revision remain unchanged

#### Scenario: Explicit persistence
- **WHEN** a user switches depth without saving and later explicitly saves
- **THEN** switching affects live rendering immediately and only saving updates the settings file used on restart
