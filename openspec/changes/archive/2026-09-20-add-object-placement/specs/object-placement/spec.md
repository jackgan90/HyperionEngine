## ADDED Requirements

### Requirement: Categorized placement palette
Editor SHALL provide Window > Place Object opening a searchable dockable palette. The palette SHALL register each type once with stable identity and multiple category memberships. It SHALL include Cube, Sphere, Cylinder, Cone, Plane, DirectionalLight, PointLight and SpotLight, with All, Basic, Shapes and Lights categories. Missing resources SHALL disable only affected entries and report the reason.

#### Scenario: Overlapping categories
- **WHEN** Cube belongs to Basic and Shapes and the user switches categories or searches All
- **THEN** both categories expose the same factory and All contains one Cube entry

#### Scenario: Layout and reopen
- **WHEN** the palette is closed and reopened from Window or the application reloads its dock layout
- **THEN** it remains a usable floating or docked panel and existing layout state is preserved

### Requirement: Reusable world placement session
Placement SHALL use a per-viewport session independent of palette widgets, with current camera ray projection, surface support offset, confirmed-miss work-plane/focus-plane fallback and explicit invalid-query handling. The session SHALL own its stable descriptor identity and document identity. It SHALL continuously render the real mesh or light icon under the pointer while over a valid viewport and SHALL own input ahead of navigation, picking and gizmos.

#### Scenario: Drag across windows
- **WHEN** the user drags a ready Sphere from an unfocused palette into the viewport and moves the pointer
- **THEN** the three-dimensional sphere preview follows the computed world position every frame without requiring a prior viewport click

#### Scenario: Leave and return
- **WHEN** a held drag leaves the viewport and returns
- **THEN** world preview hides outside and resumes inside without creating a scene object

#### Scenario: No surface or unavailable geometry
- **WHEN** the ray misses all geometry or crosses unavailable geometry
- **THEN** a confirmed miss uses the work/focus plane and unavailable or incomplete geometry prevents committing an uncertain placement

### Requirement: Isolated preview and atomic creation
Preview SHALL remain outside scene authority, saved state, scene queries, illumination, shadows and history. A valid release SHALL create one object and one undoable transaction, select the new object and preserve a continuous visual handoff. Esc, outside release, focus loss, viewport closure, modal interruption, layout/scale interruption, scene replacement and shutdown SHALL cancel without authored changes or removal of the redo branch. Stale or failed creation SHALL NOT partially modify the document.

#### Scenario: Successful creation and history
- **WHEN** a Cube is dropped then undone and redone
- **THEN** exactly one node is added, removed and restored with its transform, model reference and matching history state

#### Scenario: Cancel and save
- **WHEN** a light preview is cancelled or a save occurs without a valid drop
- **THEN** no preview object or icon is serialized and previous selection and redo history remain valid

#### Scenario: Empty document
- **WHEN** objects are placed into the initially empty document and saved with Save As
- **THEN** reload restores their model references, transforms and light components

### Requirement: Light markers and directional selection
Editor SHALL display generated, distinct PointLight, DirectionalLight and SpotLight markers at enabled objects' world origins after creation, with constant scaled screen size, viewport clipping, visibility control and matching hit testing. Directional cues SHALL reflect object orientation. The first placed directional light SHALL become the main light only if none is selected; replacing an existing main light SHALL require an explicit undoable action.

#### Scenario: Select and transform a light
- **WHEN** a visible light icon is clicked and its transform changes
- **THEN** Outliner and Details select the same scene handle and the marker/direction update with that object's world pose

#### Scenario: Existing main directional light
- **WHEN** another directional light is placed into a scene with a main light
- **THEN** the original main light remains selected until Set as main directional light is invoked and Undo restores the prior selection
