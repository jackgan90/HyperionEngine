## ADDED Requirements

### Requirement: Asset property target follows its typed member

Typed asset property registration SHALL derive the accessed canonical field and its value schema from the same reflected C++ member association. It SHALL NOT independently accept a field-target string that can disagree with the member. External operation IDs, type IDs, versions, schemas, access permissions, paging and existing shared workflow lifecycle SHALL remain compatible.

#### Scenario: Same-type property operations
- **WHEN** separate sky reference fields contain distinct values and their existing get operations are discovered and invoked
- **THEN** each operation returns the value of its own reflected member under the unchanged request/result contract

#### Scenario: Writable field follows shared workflow
- **WHEN** an existing writable property or sequence range is edited at a valid generation
- **THEN** its resolved canonical field enters the existing shared validation and transaction workflow with the same history and persistence outcome
- **AND** invalid or stale requests retain their previous rejection behavior

#### Scenario: Read-only and provider lifecycle compatibility
- **WHEN** a field is read-only, a provider is unavailable or an asynchronous operation becomes stale or is drained
- **THEN** typed identity does not add a set operation, bypass availability or generation checks, or commit work after cancellation/drain
