## MODIFIED Requirements

### Requirement: Scene-owned perspective camera state
The CPU Scene module SHALL own camera nodes with hierarchical transforms, perspective vertical field of view, near/far planes and navigation focus distance. Camera position and orientation SHALL derive from the node world transform rather than a second mutable View or Application camera. Lens and pose validation SHALL reject nonfinite values, degenerate direction bases, nonpositive near/focus distance, far not greater than near, and vertical radians outside the open interval (0.01, 3.0), without partially changing scene state.

#### Scenario: Edit a camera through Scene
- **WHEN** a client changes a camera transform and lens without constructing an application plugin
- **THEN** the next scene publication exposes those values and a scene snapshot persists them

#### Scenario: Parent transform affects the camera
- **WHEN** an ancestor of a camera moves or rotates
- **THEN** the camera uses the composed world pose while its authored local transform and lens remain unchanged

#### Scenario: Invalid ancestor pose
- **WHEN** an edit would make a descendant camera forward/up basis degenerate
- **THEN** the edit fails without partially changing the ancestor, descendant, hierarchy or revisions

### Requirement: View requests select scene cameras
Renderer SHALL resolve a scene View request from a camera Handle and view-specific target dimensions, viewport, depth convention and rendering options. An empty camera selection SHALL use the scene default camera. A stale or disabled explicit selection SHALL fall back only to a valid enabled default and expose the fallback reason. A foreign-scene Handle SHALL be rejected. With no valid camera Renderer SHALL report NoActiveCamera and SHALL NOT reuse an old camera or synthesize an invisible fallback camera.

#### Scenario: Two views select two cameras
- **WHEN** two independent view identities select different cameras from the same scene publication
- **THEN** each receives its own pose, lens, projection and visibility while both use the same published model and lighting state

#### Scenario: Active camera is removed
- **WHEN** a selected camera is removed while an earlier frame is retained
- **THEN** new frames use a valid default or report NoActiveCamera, and the retained frame keeps its original camera data

#### Scenario: No camera available
- **WHEN** the scene contains no enabled usable default or selected camera
- **THEN** the application continues input, GUI and clear output, skips scene and shadow draws, and displays the missing-camera state without presenting stale scene pixels
