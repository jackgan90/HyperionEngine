## ADDED Requirements

### Requirement: Compact viewport transform toolbar
Editor SHALL provide a single-row viewport toolbar with theme-consistent Position, Rotation and Scale icons, an active-mode indication and hover descriptions. Viewport options SHALL retain camera speed, exposure and view actions. A view-source selector SHALL use available row space and remain accessible through options when the row is too narrow.

#### Scenario: Choose a transform operation
- **WHEN** the user hovers or clicks a PRS toolbar icon
- **THEN** hovering describes its operation and clicking selects the corresponding manipulation mode with an active visual state

#### Scenario: Narrow the viewport
- **WHEN** the viewport cannot fit the inline view-source selector
- **THEN** the toolbar stays on one row and the view-source selector remains available in viewport options

## MODIFIED Requirements

### Requirement: Scoped viewport navigation
The editor SHALL reuse SceneCameraController in fly mode: WASDQE and arrow/Page aliases translate the camera only while the right mouse button is held in the viewport, and right-button dragging changes its world orientation while preserving its world position, lens and focus distance. Wheel input with the right button held SHALL adjust translation speed; wheel input without the right button SHALL retain forward/backward dolly. Home framing remains available. Navigation SHALL be isolated from menus, modal dialogs and text fields and reset on interruption.

#### Scenario: Right-button movement gate
- **WHEN** movement keys are held without a viewport right-button press, or the right button is released while a movement key remains held
- **THEN** the camera does not translate; movement while the right button is held follows the current camera direction

#### Scenario: Look from the current position
- **WHEN** the user drags with the right mouse button and no movement key is held
- **THEN** camera rotation changes with the pointer delta and camera position remains unchanged

#### Scenario: Navigate and interrupt
- **WHEN** a focused viewport receives movement or a right-button drag and subsequently loses focus or its scene is replaced
- **THEN** the camera moves using the Runtime fly mode and no held movement continues after interruption

#### Scenario: Adjust and display translation speed
- **WHEN** the right mouse button is held in the viewport and the wheel scrolls up or down
- **THEN** translation speed increases or decreases within positive finite bounds without changing the camera pose, lens or focus distance; subsequent held-key movement uses that speed and viewport options display it in scene units per second

#### Scenario: Dolly without changing translation speed
- **WHEN** the wheel scrolls without the right mouse button held
- **THEN** the camera dollies along its current forward/backward direction with the existing step behavior and the selected translation speed remains unchanged

#### Scenario: Retain speed across input interruption
- **WHEN** viewport input is reset by focus loss, a dialog, hiding the viewport or scene replacement
- **THEN** held navigation stops and the selected translation speed is retained for later navigation
