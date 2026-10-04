# object-placement Specification

## Purpose
Define categorized, reusable editor object placement with live world previews, atomic undoable creation, native resource persistence and selectable light markers.
## Requirements
### Requirement: Categorized placement palette
Editor SHALL provide Window > Place Object opening a searchable dockable palette. The palette SHALL register each type once with stable identity and multiple category memberships. It SHALL include Cube, Sphere, Cylinder, Cone, Plane, DirectionalLight, PointLight, SpotLight and SkyLight, with All, Basic, Shapes and Lights categories. SkyLight SHALL create a sky light using the Engine default sky. Missing resources SHALL disable only affected entries and report the reason.

#### Scenario: Overlapping categories
- **WHEN** Cube belongs to Basic and Shapes and the user switches categories or searches All
- **THEN** both categories expose the same factory and All contains one Cube entry

#### Scenario: Layout and reopen
- **WHEN** the palette is closed and reopened from Window or the application reloads its dock layout
- **THEN** it remains a usable floating or docked panel and existing layout state is preserved

#### Scenario: Place a sky light
- **WHEN** SkyLight is dropped into a scene without an active environment
- **THEN** one undoable node with a SkyAsset environment referencing the Engine default sky is created, selected and made the active sky light

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
Editor SHALL display generated, distinct PointLight, DirectionalLight, SpotLight and SkyLight markers at enabled objects' world origins after creation, with constant scaled screen size, viewport clipping, visibility control and matching hit testing. Directional cues SHALL reflect object orientation. The first placed directional light SHALL become the main light only if none is selected, and the first placed sky light SHALL become the active sky light only if none is selected. Replacing an existing main light or active sky light SHALL require an explicit undoable action.

#### Scenario: Select and transform a light
- **WHEN** a visible light icon is clicked and its transform changes
- **THEN** Outliner and Details select the same scene handle and the marker/direction update with that object's world pose

#### Scenario: Existing main directional light
- **WHEN** another directional light is placed into a scene with a main light
- **THEN** the original main light remains selected until Set as main directional light is invoked and Undo restores the prior selection

#### Scenario: Existing active sky light
- **WHEN** another sky light is placed into a scene with an active sky light
- **THEN** the original remains active, Details reports the new one as not effective, and Set as active sky light switches it undoably

### Requirement: Explicit placement category filtering
Placement queries SHALL represent unrestricted filtering explicitly and SHALL treat every supplied category ID as an exact extensible string identity. Display text SHALL NOT determine unrestricted behavior. Registry ordering, keyword search and unique object results SHALL remain unchanged.

#### Scenario: Unrestricted and category-specific queries
- **WHEN** a caller selects unrestricted filtering or a registered category
- **THEN** the former returns all keyword-matching objects once and the latter returns only matching category members

#### Scenario: Category named All
- **WHEN** a category with ID All is registered and selected
- **THEN** it behaves as an ordinary category independently of the palette's unrestricted All button and their controls have distinct identities

#### Scenario: Unknown and empty supplied category
- **WHEN** a supplied category has no registered members, including an empty supplied string
- **THEN** the query returns no objects instead of silently becoming unrestricted
