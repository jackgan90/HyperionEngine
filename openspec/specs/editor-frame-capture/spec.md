# editor-frame-capture Specification

## Purpose
Provide a viewport action that captures and opens the exact completed editor frame through capture orchestration shared with Viewer, with clear availability and narrow-toolbar behavior.

## Requirements
### Requirement: Viewport capture and exact replay
The viewport toolbar SHALL show a capture icon only when capture is enabled in Editor preference. Clicking an available action SHALL capture one complete editor frame including viewport rendering and GUI, and open the newly saved result in RenderDoc. Unavailable or busy actions SHALL be disabled with explanatory status.

#### Scenario: Capture succeeds
- **WHEN** the user clicks the available viewport capture icon
- **THEN** a nonempty capture containing scene, GUI and Present is saved and that exact new capture is opened in RenderDoc

#### Scenario: Disabled preference
- **WHEN** capture is disabled in Editor preference
- **THEN** the capture icon is absent immediately and no capture is requested

#### Scenario: Narrow viewport toolbar
- **WHEN** the viewport is narrowed while capture is enabled
- **THEN** fixed toolbar actions reserve their space before the flexible view selector, keeping the capture action reachable while the selector shrinks

#### Scenario: Capture or replay fails
- **WHEN** capture or replay launch fails
- **THEN** the failure is visible without crashing the editor, and capture failure does not open a stale capture

### Requirement: Shared capture orchestration
Viewer and Editor SHALL reuse engine-owned full-frame capture scope and replay logic, preserving RHI 0 execution, cancellation on exceptions, optional service lifetime and RenderDoc-disabled build isolation.

#### Scenario: Viewer compatibility
- **WHEN** Scene Viewer requests a capture with automatic opening enabled
- **THEN** it uses the same begin/end/cancel/replay implementation as Editor and retains its existing capture behavior

#### Scenario: Build without RenderDoc
- **WHEN** RenderDoc support is compiled out
- **THEN** Editor and Viewer compile and run without RenderDoc headers, library or DLL requirements
