## MODIFIED Requirements

### Requirement: Configurable rendering plugins
The engine SHALL activate rendering plugins using configuration IDs and use engine wrappers for their math, shaders and graphics work. Scene objects supplied by ModelViewer and Triangle SHALL enter through the generic render primitive registration and collection path. Plugin Main, Render and RHI responsibilities SHALL have explicit execution domains. GUI and non-scene work SHALL retain the standard pass extension path. Scene input, update and status routing SHALL use shared domain interfaces rather than concrete application casts.

#### Scenario: Plugin disabled
- **WHEN** no scene or overlay plugins are requested in the Viewer graphics profile
- **THEN** the Viewer presents only the configured clear color

#### Scenario: Two geometry producers
- **WHEN** ModelViewer or Triangle supplies scene geometry
- **THEN** the same Runtime primitive protocol handles registration, collection and removal without a viewer-specific submission dependency

#### Scenario: GUI overlay
- **WHEN** GUI is enabled over scene geometry
- **THEN** its owned draw data renders in its existing overlay pass with clipping and ordering preserved

#### Scenario: Scene update time
- **WHEN** a scene producer advances simulation or navigation
- **THEN** it receives host delta time independently of presentation and owns that advancement
