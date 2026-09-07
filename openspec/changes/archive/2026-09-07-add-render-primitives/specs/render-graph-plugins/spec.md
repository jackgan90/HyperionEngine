## MODIFIED Requirements

### Requirement: Threaded graph execution
The engine SHALL apply scene updates and collect owned frame descriptions on Render, prepare graph passes on Render, perform native resource operations and packet materialization on RHI 0, record disjoint contexts on indexed RHI threads and submit them in graph order. RHI work SHALL NOT dereference Main objects or render primitives.

#### Scenario: Triangle frame
- **WHEN** a clear pass and triangle pass are compiled
- **THEN** their command lists produce a triangle over the configured background with valid presentation transitions.

#### Scenario: Frozen scene frame
- **WHEN** Main changes its objects after a scene frame snapshot has been submitted
- **THEN** Render and RHI finish that frame using owned snapshot data and resource references

### Requirement: Configurable rendering plugins
The engine SHALL activate rendering plugins using configuration IDs and use engine wrappers for their math, shaders and graphics work. Scene objects supplied by ModelViewer and Triangle SHALL enter through the generic render primitive registration and collection path. Plugin Main, Render and RHI responsibilities SHALL have explicit execution domains. GUI and non-scene work SHALL retain the standard pass extension path.

#### Scenario: Plugin disabled
- **WHEN** no rendering plugins are requested
- **THEN** the Viewer presents only the configured clear color.

#### Scenario: Two geometry producers
- **WHEN** ModelViewer or Triangle supplies scene geometry
- **THEN** the same Runtime primitive protocol handles registration, collection and removal without a viewer-specific submission dependency

#### Scenario: GUI overlay
- **WHEN** GUI is enabled over scene geometry
- **THEN** its owned draw data renders in its existing overlay pass with clipping and ordering preserved

## ADDED Requirements

### Requirement: Scene-wide pass organization
The Renderer SHALL aggregate scene items by view, target and required rendering state instead of allocating a graph pass per model or primitive. Scene pass organization SHALL initialize depth once before dependent scene depth use and preserve it for subsequent scene draws. Blended scene items sharing a view and target SHALL be ordered across model boundaries using the documented stable center-depth policy.

#### Scenario: Many primitives in one scene
- **WHEN** the primitive count exceeds the backend recording-context count but the aggregate pass count remains supported
- **THEN** the scene renders without consuming one recording context per primitive

#### Scenario: Shared scene depth
- **WHEN** independently registered models overlap in a depth-enabled scene
- **THEN** later model draws do not clear earlier scene depth and nearer opaque surfaces occlude farther surfaces
