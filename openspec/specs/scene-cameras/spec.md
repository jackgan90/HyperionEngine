# scene-cameras Specification

## Purpose
Define Scene-owned perspective camera state, validated hierarchical poses, view selection and controller edits, with consistent renderer camera data and persistent scene snapshots.
## Requirements
### Requirement: Scene-owned perspective camera state
The CPU Scene module SHALL own camera nodes with hierarchical transforms, perspective vertical field of view, near/far planes and navigation focus distance. Camera position and orientation SHALL derive from the node world transform rather than a second mutable View or Application camera. Lens and pose validation SHALL reject nonfinite values, degenerate direction bases, nonpositive near/focus distance, far not greater than near, and vertical radians outside the open interval (0.01, 3.0), without partially changing scene state.

#### Scenario: Edit a camera through Scene
- **WHEN** a client changes a camera transform and lens without constructing a viewer plugin
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
- **THEN** the viewer continues input, GUI and clear output, skips scene and shadow draws, and displays the missing-camera state without presenting stale scene pixels

### Requirement: Renderer derives consistent camera data
Renderer SHALL compute Eye, ViewProjection and shadow camera information from the same resolved camera pose and lens. Effective viewport aspect and the View depth convention SHALL determine projection. The CPU pose convention SHALL use local minus Z forward and plus Y up, transforming and orthonormalizing these axes as specified in design D3. Nonuniform and mirrored transforms SHALL NOT introduce scale or shear into the view basis. Zero-sized targets SHALL follow the skipped-frame path.

#### Scenario: Lens and target changes
- **WHEN** FOV, near/far, viewport dimensions or depth convention changes
- **THEN** drawing, main-view culling, transparent ordering and CSM use consistent current camera-derived data

#### Scenario: Scaled camera parent
- **WHEN** a camera inherits a finite nonuniform or mirrored transform with a valid direction basis
- **THEN** its extracted forward/up are finite and orthonormal and its world position follows the parent transform

### Requirement: Input controllers edit the authoritative camera
Orbit, dolly, translation, fit and benchmark controls SHALL read and edit the selected scene camera through Scene operations. Controllers SHALL retain only interaction policy and transient input state, and SHALL NOT unconditionally overwrite scene pose or lens from stale controller values. World-space edits under a parent SHALL convert to local space or fail explicitly when inversion is invalid.

#### Scenario: External edit followed by idle input
- **WHEN** an external client or hierarchy operation changes a camera and the controller receives no movement input
- **THEN** subsequent ticks preserve the externally edited camera

#### Scenario: Fit a parented camera
- **WHEN** Fit is requested for a camera under a transformed parent
- **THEN** the camera frames effective visible model bounds using its current lens and viewport, updates its local transform and focus distance, and leaves other nodes unchanged
