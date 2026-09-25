## MODIFIED Requirements

### Requirement: Render ownership and independent type extension
The Render domain SHALL exclusively construct, register, mutate and destroy fully constructed primitives. A render scene SHALL exclusively own its registered primitives. Main objects SHALL use opaque bindings and SHALL NOT dereference primitive state. Primitive implementations SHALL NOT retain or dereference mutable Main-thread logical objects. Main and Render type hierarchies SHALL be independently extensible through engine-owned interfaces.

#### Scenario: Main wrapper is destroyed
- **WHEN** a Main wrapper is destroyed after its create command was accepted but before Render processes it
- **THEN** accepted work uses owned data, removal completes safely, and every fully constructed primitive is destroyed exactly once on Render

#### Scenario: Independent primitive implementation
- **WHEN** a test supplies a different primitive implementation through the generic creation and collection interface
- **THEN** it renders or collects without a concrete application dependency or a matching Main-side inheritance hierarchy

#### Scenario: Wrong-domain access
- **WHEN** a guarded primitive mutation or scene registration is invoked outside Render
- **THEN** the execution-domain contract violation is detected before mutable render state is accessed
