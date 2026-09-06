# runtime-module-organization Specification

## Purpose
Keep runtime features, native backends, experiment plugins and applications in cohesive modules with explicit public interfaces and private dependency boundaries.
## Requirements
### Requirement: Cohesive module ownership
Each runtime, backend and algorithm module SHALL group its public API and private implementation beneath its own concept directory and own its build definition. Public include roots SHALL not expose other modules' private headers.

#### Scenario: Browse and extend a module
- **WHEN** a developer opens the generated Visual Studio solution
- **THEN** foundation, renderer, backend, plugin, application and test projects are grouped by responsibility and show their module-local headers and implementations

### Requirement: Dependency direction and future scene systems
Repository guidance SHALL place Scene and Animation as independent runtime modules producing data for Renderer and SHALL prohibit their direct dependency on native graphics backends. Core utilities SHALL remain in focused foundation folders. Automated checks SHALL enforce current module ownership and include boundaries.

#### Scenario: Add scene or animation functionality
- **WHEN** a contributor follows the module organization guidance
- **THEN** scene/animation data processing can use foundation and asset services without requiring render commands or backend includes

### Requirement: Third-party isolation follows module boundaries
Vendor APIs SHALL remain in module-private adapters or native backend private implementations; application code, algorithms and public interfaces SHALL use engine wrappers.

#### Scenario: Accidental backend include
- **WHEN** a runtime module or algorithm plugin includes a concrete backend header or vendor header
- **THEN** boundary validation fails and identifies the offending source
