## ADDED Requirements

### Requirement: Cohesive Editor ownership and isolated acceptance driver
Editor SHALL keep feature lifecycle and typed service registration in its plugin while assigning viewport behavior and pending document transitions to focused private owners. Acceptance-only state and routing SHALL live behind a distinct driver with semantic phases and explicit input-window selection. Production input routing SHALL not interpret numeric acceptance-step ranges. A BUILD_TESTING-disabled Editor SHALL compile and link without acceptance implementation sources.

#### Scenario: Test-disabled build
- **WHEN** Editor is configured with BUILD_TESTING disabled
- **THEN** it builds and links without acceptance implementation and reports requested unavailable acceptance behavior as a controlled diagnostic

#### Scenario: Test-enabled lifecycle
- **WHEN** existing Editor acceptance scenarios run with tests enabled, including optional provider absence and shutdown
- **THEN** their input targets, useful coverage, provider lifetime and work-drain behavior remain intact

#### Scenario: Pending transition and rendering
- **WHEN** a dirty document transition is confirmed or cancelled while viewport work exists
- **THEN** existing busy/discard decisions, document identity, frame snapshot ownership and GPU retirement behavior remain unchanged
