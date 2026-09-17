# scene-editor Specification

## Purpose
Provide a docked scene editor with mounted scene opening, an embedded rendered viewport, reusable fly-camera navigation, and safe input and resource lifetimes.
## Requirements
### Requirement: Docked editor workspace
The engine SHALL provide a standalone editor with a compact charcoal and blue-accent workspace, menu bar, docked scene viewport, Outliner, Details, scene browser, and persisted resettable layout.

#### Scenario: Launch and rearrange
- **WHEN** the editor starts and the user moves or resizes panels
- **THEN** panels can dock inside the main window and the layout can be restored on the next launch or reset to the default

### Requirement: Open mounted scenes
The editor SHALL let the user choose File > Open Scene and select a native scene from mounted content, including Sponza, and SHALL display loading status and recoverable errors.

#### Scenario: Open Sponza
- **WHEN** the user selects Sponza and confirms opening
- **THEN** its scene loads asynchronously, nodes appear in the Outliner and its rendered image appears inside the viewport

#### Scenario: Failed scene then valid scene
- **WHEN** opening a scene fails and the user subsequently opens a valid scene
- **THEN** the error is visible, the editor stays usable and the valid scene can load

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
- **THEN** translation speed increases or decreases within positive finite bounds without changing the camera pose, lens or focus distance; subsequent held-key movement uses that speed and the viewport toolbar displays it in scene units per second

#### Scenario: Dolly without changing translation speed
- **WHEN** the wheel scrolls without the right mouse button held
- **THEN** the camera dollies along its current forward/backward direction with the existing step behavior and the selected translation speed remains unchanged

#### Scenario: Retain speed across input interruption
- **WHEN** viewport input is reset by focus loss, a dialog, hiding the viewport or scene replacement
- **THEN** held navigation stops and the selected translation speed is retained for later navigation

### Requirement: Embedded viewport lifetime
The scene image SHALL follow the viewport content pixel size and declare texture writes and reads through RenderGraph while retaining submitted resource generations.

#### Scenario: Resize and close
- **WHEN** the viewport is resized, hidden, restored or the editor closes during loading
- **THEN** rendering remains valid and resources and pending work are retired safely

### Requirement: Editable document history and save points
Editor SHALL provide component property and object structural editing, undo/redo, native scene save/save-as and a document dirty indicator. Save SHALL capture immutable authored state, and asynchronous completion SHALL not mark subsequent edits as saved. Asset replacement SHALL use explicit reference resolution and reject stale results.

#### Scenario: Edit undo save reload
- **WHEN** an instance property is edited, undone, redone, saved and reopened
- **THEN** its final authored value and independent identity survive without changing other instances

#### Scenario: Edit while saving
- **WHEN** another edit is committed after a save captures its snapshot
- **THEN** successful save completion leaves the document dirty

### Requirement: Independent editor camera
Editor viewport navigation SHALL retain the established fly input and speed behavior while using an independent pose/lens override. Ordinary navigation SHALL not mutate authored scene cameras or document history.

#### Scenario: Browse and save
- **WHEN** the user navigates the editor viewport and saves without editing scene content
- **THEN** scene camera transforms and settings remain unchanged
