# scene-viewer-camera-motion Specification

## Purpose
TBD - created by archiving change smooth-scene-viewer-camera. Update Purpose after archive.
## Requirements
### Requirement: Continuous view-relative translation
SceneViewer SHALL translate its active enabled camera using held W/S along camera forward/back, A/D along camera left/right and Q/E along world down/up. Arrows and Page Up/Down SHALL remain continuous aliases. Translation SHALL integrate elapsed seconds once per frame independently of key-repeat events, preserve camera orientation/lens/focus, cancel opposing directions and normalize combined world movement to avoid diagonal or alias speed boosts.

#### Scenario: Held movement between events
- **WHEN** W is pressed once and subsequent frames contain no key events
- **THEN** every frame advances along the camera forward vector, including its pitch, until release

#### Scenario: Frame-rate independence and combined input
- **WHEN** equal elapsed time is simulated at 30, 60 and 120 Hz with single, diagonal or duplicate alias input
- **THEN** travel distance is equal within floating-point tolerance and combined input never exceeds single-direction speed

### Requirement: Safe input interruption
SceneViewer SHALL clear held movement on focus loss, GUI keyboard capture, minimization and stop. Key release SHALL stop that direction even if GUI captures the release. Repeat events SHALL NOT re-arm cleared keys. Invalid or nonpositive delta SHALL cause no movement; valid delta SHALL be capped at 0.1 seconds. Missing or disabled cameras SHALL be harmless.

#### Scenario: Focus or GUI interruption
- **WHEN** movement is interrupted by focus loss or keyboard capture and only repeat events follow
- **THEN** movement remains stopped until a fresh uncaptured press

#### Scenario: Minimized window and long frame
- **WHEN** the Viewer skips a minimized frame or resumes after a long stall
- **THEN** held movement is cleared during minimization and any movement update is bounded to 0.1 seconds

### Requirement: Reusable camera controls
The runtime SHALL expose a controller independent of SceneViewer, GUI and application implementations, covering keyboard movement, RMB orbit and wheel dolly. Hosts SHALL supply engine input events, capture flags and elapsed seconds and SHALL reset it on viewport deactivation or scene replacement. The controller SHALL borrow scene state without owning it.

#### Scenario: Another viewport uses navigation
- **WHEN** an editor or another plugin creates a controller and supplies its scene and input
- **THEN** the same navigation works without linking SceneViewer or duplicating gesture logic

#### Scenario: Mouse capture interrupts orbit
- **WHEN** GUI captures mouse input during a right-button drag
- **THEN** orbit and wheel navigation are suppressed until capture ends and orbit requires a fresh right-button press
