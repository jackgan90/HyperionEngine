## ADDED Requirements

### Requirement: View-consistent geometric ray
Viewport picking SHALL use the active editor or preview camera and the rendered viewport pixel aspect with near/far clipping for either depth convention. An invalid camera or viewport SHALL produce Unavailable. The contract SHALL describe current Main static geometry rather than shader-accurate displayed pixels.

#### Scenario: Resized preview camera
- **WHEN** a valid scene camera is previewed after viewport resize or DPI change
- **THEN** picking uses that camera and the updated render extent

### Requirement: Click ownership and selection
Editor SHALL select a model on an eligible left-button release after a small-movement click. Gizmos, navigation, modal/text UI and viewport lifetime transitions SHALL take priority. Confirmed empty space SHALL clear selection persistently, while unavailable queries SHALL preserve it.

#### Scenario: Gizmo or navigation consumes gesture
- **WHEN** a gizmo drag, right-button navigation, focus loss or modal UI intervenes between press and release
- **THEN** that gesture does not select another model

#### Scenario: Empty click and outliner
- **WHEN** an eligible click misses all available geometry
- **THEN** selection is cleared and subsequent outliner frames do not automatically reselect a model

#### Scenario: Select geometry after an Outliner light
- **WHEN** a light is selected in Outliner and the user presses on model geometry outside its gizmo, holds still for multiple frames and releases
- **THEN** the model becomes the selection shared by Outliner, Details and the gizmo, without the image background taking ownership of the gesture

### Requirement: Selection is not an authored edit
Viewport selection SHALL reuse the existing selection and inspector transition path, create no document mutation or history entry, and cancel pending gestures when scenes or views change.

#### Scenario: Select then save or undo
- **WHEN** a user changes selection through the viewport
- **THEN** document dirty state and undo history remain unchanged
