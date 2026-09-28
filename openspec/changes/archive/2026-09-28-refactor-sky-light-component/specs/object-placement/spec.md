## MODIFIED Requirements

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
