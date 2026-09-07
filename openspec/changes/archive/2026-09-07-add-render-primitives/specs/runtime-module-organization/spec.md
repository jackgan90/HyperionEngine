## MODIFIED Requirements

### Requirement: Dependency direction and future scene systems
Repository guidance SHALL place Scene and Animation as independent runtime modules producing data for Renderer and SHALL prohibit their dependency on Renderer, RHI or native graphics backends. Main-side rendering bindings and asset-to-render adapters SHALL live in Renderer or a higher-level bridge that depends on the data modules. Core utilities SHALL remain in focused foundation folders. Automated checks SHALL enforce current module ownership and include boundaries.

#### Scenario: Add scene or animation functionality
- **WHEN** a contributor follows the module organization guidance
- **THEN** scene/animation data processing can use foundation and asset services without requiring render commands or backend includes

#### Scenario: Register a model for rendering
- **WHEN** an application binds an immutable Scene model asset to render primitives
- **THEN** the binding layer depends on Scene and Renderer while the Scene target and its public asset types remain independent of Renderer/RHI

## ADDED Requirements

### Requirement: Application-independent rendering contracts
Primitive interfaces, Main rendering bindings, scene collection and shared-resource coordination SHALL be owned by Runtime modules, independent of ModelViewer, Triangle and concrete graphics backends. Project documentation SHALL define thread ownership, message data lifetime, frame boundaries, registration identity, failure cleanup and the distinction between scene primitives and non-scene passes.

#### Scenario: Runtime-only primitive test
- **WHEN** a test registers and collects a primitive using generic Renderer interfaces
- **THEN** no ModelViewer or Triangle module is required

#### Scenario: Contributor follows the rendering contract
- **WHEN** a contributor reads the render primitive and module architecture documentation
- **THEN** construction, mutation, collection, destruction, GPU retirement and extension responsibilities have explicit owning domains and dependency directions
