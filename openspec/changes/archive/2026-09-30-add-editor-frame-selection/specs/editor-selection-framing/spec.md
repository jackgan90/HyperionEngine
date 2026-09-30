## ADDED Requirements

### Requirement: Shared selection bounds
Selection framing SHALL use the union of world-space bounds for every selected subtree, visiting overlapping selected subtrees only once. It SHALL preserve model SourceNode/SourcePrimitive geometry boundaries and affine world transforms. Hidden or disabled selected models with available geometry SHALL remain frameable. Groups with descendants SHALL use descendant bounds; non-geometric leaves and empty groups SHALL use a stable world-position-centered minimum extent independent of light influence range.

#### Scenario: Multiple selected models
- **WHEN** two separated models are selected in any order
- **THEN** framing uses the center of their union bounds and the same result regardless of primary selection

#### Scenario: Selected parent and child
- **WHEN** a group and one of its descendants are selected
- **THEN** the descendant contributes once and the group origin does not expand usable descendant bounds

#### Scenario: Affine and non-geometric objects
- **WHEN** models have rotated or nonuniformly scaled parents, or selected objects are lights, cameras or empty groups
- **THEN** framing uses transformed geometry or stable fallback bounds and produces a finite valid camera

### Requirement: Temporary bounds fitting
The shared Renderer framing algorithm SHALL preserve browsing-camera orientation and field of view, center its focus on the target bounds, and compute a positive finite distance that fits the bounds at the viewport aspect ratio with padding. Selection framing SHALL fit temporary clipping planes as needed and reset held navigation. It SHALL NOT change scene selection, authored nodes/settings, revision, dirty state or history. Empty selection SHALL be a no-op.

#### Scenario: Wide or narrow viewport
- **WHEN** a valid selection is framed in a wide or narrow viewport
- **THEN** its bounds fit inside the viewing frustum with the current orientation/FOV and valid clipping planes

#### Scenario: Document invariance
- **WHEN** the user frames a selection and saves the scene normally
- **THEN** authored snapshot, revision, dirty state and undo history remain unchanged by framing

#### Scenario: Empty selection
- **WHEN** framing is requested without selected objects
- **THEN** the camera and document remain unchanged

### Requirement: Focus-aware F shortcut
Editor SHALL invoke the shared selection-framing operation on an unmodified non-repeat F key-down while Viewport, Outliner or Details owns focus and the initialized viewport is visible. Text ownership, popups, focus loss, active placement/reparent/gizmo/camera gestures and blocked document transitions SHALL prevent shortcut invocation. Scene-camera preview SHALL retain strict isolation and refuse selection framing.

#### Scenario: Viewport and Outliner selection
- **WHEN** an object is selected by clicking the viewport or an Outliner row and F is pressed in that scene panel
- **THEN** the independent browsing camera frames the shared selection

#### Scenario: Search and blocked interaction
- **WHEN** F is typed in a search/property text field, received during a popup/gesture, repeated, modified or pressed in Content Browser
- **THEN** selection framing does not change the camera

#### Scenario: Scene-camera preview
- **WHEN** F is pressed while previewing an authored scene camera
- **THEN** both the retained browsing view and authored camera are unchanged

### Requirement: Toolbar and full-scene compatibility
Editor SHALL remove the dedicated main-toolbar Frame Scene button and document F selection framing. Home full-scene framing and the existing `view.frame_scene` operation SHALL remain available with their existing temporary-state semantics.

#### Scenario: Full-scene framing retained
- **WHEN** Home is pressed in the browsing viewport or an agent calls `view.frame_scene`
- **THEN** whole-scene framing remains usable without the toolbar button

#### Scenario: Selection framing followed by full-scene framing
- **WHEN** a small selected object is framed and Home or `view.frame_scene` subsequently frames a larger visible scene
- **THEN** the temporary near and far planes fit the full-scene target and its geometry remains inside the viewing frustum

### Requirement: Typed automation parity
`view.frame_selection` SHALL be discoverable and callable through the existing typed catalog using reflected document/revision input and viewport-state output. GUI and automation SHALL call the same UI-independent provider operation on Main. The service SHALL reject stale document/revision, busy preparation/interaction and unavailable preview/provider state without partial camera or document changes. Completion SHALL mean temporary state is committed on Main for subsequent rendered frames.

#### Scenario: Discover and invoke
- **WHEN** an agent searches/describes the operation and calls it for the current document/revision
- **THEN** its schemas/effects/completion are available and the same camera as the GUI operation is visible to another connection

#### Scenario: Stale busy or absent provider
- **WHEN** the request refers to an old document/revision, the scene is busy, or a viewport/document provider is absent
- **THEN** the operation reports a controlled error and leaves camera, selection and document unchanged

#### Scenario: Camera navigation or ordinary popup is active
- **WHEN** an agent calls `view.frame_selection` during RMB camera navigation or an ordinary viewport popup
- **THEN** the shared provider reports `busy` without moving the camera or interrupting the active navigation
