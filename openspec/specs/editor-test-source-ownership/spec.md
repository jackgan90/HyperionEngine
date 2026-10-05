# editor-test-source-ownership Specification

## Purpose
Define Editor-owned test source and header boundaries and verify actual acceptance build selection without changing production or scenario behavior.

## Requirements
### Requirement: Explicit Editor test ownership

Editor unit test entry points and acceptance scenario implementation/state SHALL reside in Editor-owned Tests directories. Production Private code SHALL depend only on the acceptance driver contract, observations, reporting and unavailable implementation, without including test implementation or scenario state.

#### Scenario: Production include leaks into a test header

- **WHEN** a production source directly or transitively includes a Tests header
- **THEN** the boundary checker rejects the dependency with its source path

#### Scenario: New scenario has a different filename

- **WHEN** an acceptance implementation is added to the dedicated test directory
- **THEN** its ownership and build selection are checked independently of an Acceptance filename pattern

#### Scenario: Test helper has dependencies selected only by its test target

- **WHEN** a module-local Tests header is consumed by a configured test target with its own dependencies
- **THEN** the checker does not attribute those includes to the module's production target
- **AND** explicit header sources and transitive local includes identify the actual consuming targets
- **AND** a production consumer still requires its own direct dependency, public exports retain visibility requirements, and test helpers retain private-header and vendor isolation

#### Scenario: A test target explicitly lists an exported header

- **WHEN** a test target lists a module Public header as an explicit source
- **THEN** the header retains its public export and isolation rules
- **AND** the global test-support include exception cannot bypass rejection of a public dependency on test headers

### Requirement: Preserve configured acceptance selection

BUILD_TESTING=ON SHALL compile the existing acceptance driver, harness and scenarios. BUILD_TESTING=OFF SHALL compile no Editor Tests source into the production plugin and SHALL select the controlled unavailable driver. Existing target names, CLI flags, production startup, reports and scenario behavior MUST remain unchanged.

#### Scenario: Testing enabled

- **WHEN** Editor is configured and built with testing enabled
- **THEN** actual target and compiler inputs contain all acceptance implementations and exclude the unavailable implementation
- **AND** existing automated Editor acceptance scenarios run successfully

#### Scenario: Testing disabled

- **WHEN** Editor is configured and built with testing disabled
- **THEN** actual plugin/compiler inputs contain no Editor Tests source
- **AND** normal application startup remains usable while an exercise request returns the existing controlled unavailable failure
