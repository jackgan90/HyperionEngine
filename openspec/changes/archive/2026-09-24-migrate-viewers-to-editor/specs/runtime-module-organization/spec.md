## MODIFIED Requirements

### Requirement: Application-independent rendering contracts
Primitive interfaces, Main rendering bindings, scene collection and shared-resource coordination SHALL be owned by Runtime modules, independent of application plugins, Triangle and concrete graphics backends. Project documentation SHALL define thread ownership, message data lifetime, frame boundaries, registration identity, failure cleanup and the distinction between scene primitives and non-scene passes.

#### Scenario: Runtime-only primitive test
- **WHEN** a test registers and collects a primitive using generic Renderer interfaces
- **THEN** no application or Triangle module is required

#### Scenario: Contributor follows the rendering contract
- **WHEN** a contributor reads the render primitive and module architecture documentation
- **THEN** construction, mutation, collection, destruction, GPU retirement and extension responsibilities have explicit owning domains and dependency directions
