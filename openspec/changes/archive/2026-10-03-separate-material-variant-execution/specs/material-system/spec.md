## ADDED Requirements

### Requirement: Material variant identity and execution mode are independent
Renderer material variant requests and compiled passes SHALL carry an explicit Ordinary or Instanced execution mode independently of their open variant names. Names SHALL identify variants without inferring execution mode. Requests SHALL default to Ordinary, reject unknown modes, preserve unique usage/name identities, and reject multiple Instanced candidates for one usage. Compiled material cache identity SHALL include execution mode as well as existing semantic inputs.

#### Scenario: Name does not activate instancing
- **WHEN** an Ordinary request is named `Instance`
- **THEN** it compiles with ordinary bindings and capacity and is not returned as an instance candidate

#### Scenario: Custom instance identity
- **WHEN** an Instanced request has an arbitrary nonempty name and a valid reflected instance contract
- **THEN** compilation and typed instance lookup use its mode while name lookup preserves its authored identity

#### Scenario: Invalid or ambiguous policy
- **WHEN** a request uses an unknown execution mode or multiple Instanced candidates for the same usage
- **THEN** material preparation rejects the request before publishing a compiled program

#### Scenario: Semantic cache separation
- **WHEN** otherwise equal requests differ in execution mode
- **THEN** their compiled material identities differ and existing shader, binding and packet caches cannot substitute incompatible programs
