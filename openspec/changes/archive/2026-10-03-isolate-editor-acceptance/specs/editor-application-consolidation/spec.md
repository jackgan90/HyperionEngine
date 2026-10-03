## MODIFIED Requirements

### Requirement: Cohesive Editor ownership and isolated acceptance driver
Editor SHALL keep feature lifecycle and typed service registration in its plugin while assigning viewport behavior and pending document transitions to focused private owners. Acceptance scenario methods, steps, assertions, fixture identifiers and snapshots SHALL belong to a private acceptance implementation behind an opaque driver. Production input, drawing and frame routing SHALL use semantic observations or explicit execution/capture policy rather than inspect scenario state or numeric step ranges. A BUILD_TESTING-disabled Editor SHALL compile and link without acceptance implementation sources or scenario-state storage. Existing acceptance CLI flags and report keys/value types SHALL remain compatible.

#### Scenario: Test-disabled build
- **WHEN** Editor is configured with BUILD_TESTING disabled
- **THEN** it builds and links without acceptance implementation and reports requested unavailable acceptance behavior as a controlled diagnostic
- **AND** normal finite-frame execution, reports, captures and benchmark behavior remain available

#### Scenario: Test-enabled lifecycle
- **WHEN** existing Editor acceptance scenarios run with tests enabled, including optional provider absence and shutdown
- **THEN** their input targets, useful coverage, provider lifetime and work-drain behavior remain intact
- **AND** existing assertions, report fields and synthetic input timing retain their meaning

#### Scenario: Pending transition and rendering
- **WHEN** a dirty document transition is confirmed or cancelled while viewport work exists
- **THEN** existing busy/discard decisions, document identity, frame snapshot ownership and GPU retirement behavior remain unchanged

#### Scenario: Production observations and scenario ownership
- **WHEN** an Outliner row, content tile, property or other observed control is drawn, or a frame/capture completes
- **THEN** production supplies semantic observations or owned effect inputs without recognizing fixture IDs, individual case phases or assertion rules
- **AND** the acceptance implementation retains the data needed to exercise and verify the existing behavior
