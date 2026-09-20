# scene-transform-gizmos Specification

## Purpose
Provide reusable viewport position, rotation and scale manipulation with defined affine degeneracy handling, valid pointer input and transactional Editor integration.
## Requirements
### Requirement: Reusable scene transform controller
The engine SHALL provide a per-viewport CPU transform controller through engine-owned camera, matrix, mode, handle and drawing-data types. It SHALL compute hit testing and candidate local transforms without owning scene nodes or depending on GUI, native backends or concrete Editor plugins.

#### Scenario: Host draws a selected transform
- **WHEN** a host supplies valid camera, viewport, local and parent transforms
- **THEN** the controller returns projected geometry centered at the object's world origin and sized in logical UI coordinates

### Requirement: Selected object PRS overlay
Editor SHALL show one active PRS overlay for the primary selected object in the editor view. X/Y/Z handles SHALL use red/green/blue, with a highlighted hovered or active handle, and SHALL be clipped to the viewport image. The selection and highlighting capabilities SHALL supply the shared ordered selection.

#### Scenario: Select and change mode
- **WHEN** an object is selected in Outliner and the user chooses Position, Rotation or Scale
- **THEN** the matching arrow, ring or scale-handle overlay appears at its world origin

#### Scenario: Preview an authored camera
- **WHEN** the viewport is showing an authored scene-camera preview
- **THEN** transform manipulation and its overlay are inactive

### Requirement: World and camera-plane translation
Position mode SHALL translate along a selected world X/Y/Z axis or freely in the camera plane through its center handle, preserving the initial local linear matrix. It SHALL convert world displacement through an invertible parent transform and disable translation for a singular parent.

#### Scenario: Translate under a transformed parent
- **WHEN** an axis is dragged under an invertible mirrored, nonuniform or sheared parent
- **THEN** world displacement follows that axis and the local linear matrix remains unchanged

#### Scenario: Collapsed parent
- **WHEN** the parent transform has no valid inverse
- **THEN** position handles are disabled and dragging does not modify the node or crash

### Requirement: Local-axis rotation
Rotation mode SHALL rotate around the selected decomposed local rotation axis while preserving position, signed scale and shear. Near edge-on rings SHALL use the initial projected tangent; an unusable tangent SHALL prevent starting that handle. Input through the ring center SHALL not change the angle.

#### Scenario: Rotate an affine object
- **WHEN** the user drags a valid colored rotation ring
- **THEN** local rotation changes while its position and affine stretch components are retained

### Requirement: Signed scale and defined degeneracy
Scale mode SHALL adjust selected decomposed local scale components relative to the drag-start snapshot and allow crossing zero into mirrored scale. An initially zero component SHALL use unit sensitivity for recovery. The center handle SHALL scale raw shear entries together with scale components so nonzero initial components produce proportional scaling of the complete linear matrix. The basis SHALL remain fixed during the drag; nonfinite or invalid output SHALL not be applied.

#### Scenario: Uniform scaling of a sheared object
- **WHEN** an object with nonzero initial scale and shear is center-scaled by two, zero or a negative factor
- **THEN** its complete linear matrix is multiplied by that factor while its position stays fixed

#### Scenario: Recover a zero component
- **WHEN** a scale component is initially zero and its handle is dragged
- **THEN** it can return to a finite nonzero value using unit sensitivity without division by the initial scale

#### Scenario: Fully collapsed object or camera-aligned axis
- **WHEN** all linear components are zero or an axis has no usable projected drag direction
- **THEN** the existing deterministic affine basis is used for the collapsed transform and unusable projected axis handles do not start a drag

### Requirement: Transactional viewport manipulation
Editor SHALL apply live transform previews without per-frame history entries, commit one undoable command on completion and mark changed previews dirty. Esc SHALL restore the exact initial transform and retain the existing redo branch. Compatible unrelated node-property updates SHALL survive finishing the drag; a conflicting external transform SHALL not be overwritten.

#### Scenario: Complete, undo and redo
- **WHEN** the user drags a handle through multiple preview frames and releases it
- **THEN** one command records the final transform, undo restores the initial matrix and redo restores the final matrix

#### Scenario: Cancel or do nothing
- **WHEN** a drag is canceled with Esc or finishes without a transform change
- **THEN** no new history entry is added and cancel preserves the previous redo branch

### Requirement: Interrupted input retains valid preview
Editor SHALL finish active manipulation when focus or pointer validity is lost, the viewport is hidden or resized, selection/mode changes, or a save begins. Invalid pointer sentinels SHALL never be evaluated as drag samples. Camera navigation SHALL be isolated during manipulation, and valid outside-viewport release SHALL remain supported.

#### Scenario: Focus loss during PRS manipulation
- **WHEN** GUI processes a focus-loss event while a changed PRS preview is active
- **THEN** the last valid preview is committed once without applying the unavailable pointer position, and undo/redo restore the initial/final matrices

#### Scenario: Release outside the image
- **WHEN** the pointer leaves the viewport while captured and releases at valid coordinates
- **THEN** the interaction finishes normally without moving the viewport window or browsing camera

### Requirement: Primary-centered group transform
Multi-selection gizmos SHALL apply a common snapshot-relative world transformation around the primary origin. The primary's final world matrix SHALL match its single-selection result, including affine effects of nonuniform parents. Other objects SHALL change positions and linear transforms together. Selected descendants SHALL inherit the operation once. Undefined group scale ratios and required singular parent mappings SHALL reject the affected operation without partial changes; single-object zero-scale recovery and Details absolute assignment SHALL remain available.

#### Scenario: Translate a hierarchy once
- **WHEN** a parent and child are selected and translated by a world displacement
- **THEN** both world positions change by that displacement exactly once

#### Scenario: Rotate and scale around primary
- **WHEN** two independent objects are rotated or scaled using the primary gizmo
- **THEN** the primary origin stays fixed and the other object's offset and orientation or shape follow the same affine operation

#### Scenario: Initially zero group scale
- **WHEN** a group scale operation would require an undefined ratio from an initially zero primary scale
- **THEN** that operation is unavailable with an explanation, while a gesture starting from valid scale can cross zero into negative scale
