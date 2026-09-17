# scene-browsing-views Specification

## Purpose
Separate temporary viewport navigation and optional initial browsing presets from authored scene cameras, with explicit preview and undoable camera authoring.

## Requirements
### Requirement: Initial browsing view is separate scene metadata
The scene SHALL support an optional validated pose/lens browsing preset independent of scene objects and runtime default-camera selection. Old scene files SHALL load without changing topology or implicitly copying an authored camera into the preset.

#### Scenario: Preset round trip
- **WHEN** a scene with an initial browsing preset is saved and reopened
- **THEN** the preset retains its pose/lens and the scene contains exactly its authored objects

#### Scenario: Old camera-containing scene
- **WHEN** an older scene with a default camera and no preset is loaded
- **THEN** that camera remains unchanged, the preset is absent, and browsing uses deterministic framing

### Requirement: Navigation is temporary
Editor and SceneViewer SHALL navigate independent viewport values initialized from the preset or deterministic framing. Ordinary navigation and scene saving SHALL NOT modify the preset, authored camera transforms, or scene revision.

#### Scenario: Camera-free browsing
- **WHEN** a scene without camera objects is loaded and the user navigates
- **THEN** both hosts render normally without adding objects and the authored snapshot remains unchanged

#### Scenario: Explicit initial-view update
- **WHEN** the user sets the editor view as the initial view and saves
- **THEN** the change is undoable, marks the document dirty, and controls the next opening view

### Requirement: Scene-camera preview is explicit and strict
Editor SHALL expose the current view source and an explicit preview of an authored camera, retaining the independent editor view. Preview SHALL use the referenced camera's latest committed pose/lens and SHALL NOT navigate or silently fall back to another camera.

#### Scenario: Camera property edit
- **WHEN** the previewed camera's pose or lens is committed
- **THEN** the next published preview reflects the change

#### Scenario: Disabled or removed preview camera
- **WHEN** the preview target is disabled, deleted, or loses its Camera component
- **THEN** its scene preview stops, the UI explains its unavailable state, and an explicit return action restores the editor view

### Requirement: Explicit camera authoring from the editor view
Editor SHALL allow applying its retained view to an authored camera and creating a camera from that view using validated undoable document edits. Parented targets SHALL preserve the requested world pose through a valid parent inverse.

#### Scenario: Apply then undo
- **WHEN** the user applies the editor view to a selected camera and then undoes it
- **THEN** the camera and scene settings return to their previous values and the editor view is unchanged

#### Scenario: Singular parent rejection
- **WHEN** a parent transform cannot express the requested world camera pose
- **THEN** the operation reports an error without modifying the document or history

### Requirement: Sponza migration preserves content
Shipped Sponza SHALL migrate its verified browsing-only default camera into InitialView through an explicit guarded publication. Other objects, shared dependency files, model references, and appearance SHALL remain unchanged.

#### Scenario: Unsafe migration input
- **WHEN** the candidate camera has additional components, descendants, or conflicting references/state
- **THEN** migration refuses to discard that state

#### Scenario: Published camera-free scene
- **WHEN** the migrated Sponza is opened in Editor and SceneViewer
- **THEN** it starts at the previous saved camera framing, renders its model/lights, and contains no placeholder camera object
