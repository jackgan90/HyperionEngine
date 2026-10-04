## ADDED Requirements

### Requirement: Typed screenshot window routing
Native screenshot admission, routing, polling and asset-window closure SHALL use typed known window identity. Renderer SHALL own the single mapping for `main` and `assets`, and the default SHALL remain Main.

#### Scenario: Main and asset output
- **WHEN** an accepted request targets main or assets, or omits the window field
- **THEN** the existing window produces its PNG with unchanged completion metadata, with omission selecting main
- **AND** readback, write completion, pending ownership and closure behavior remain unchanged

### Requirement: Open screenshot window protocol
The request SHALL retain the existing string wire/archive representation, `main` default, field description, requiredness and schema. Unknown strings SHALL round trip without normalization or truncation and have no known native kind.

#### Scenario: Compatibility inputs
- **WHEN** known, empty, unknown, differently cased, whitespace-bearing or embedded-NUL window strings are decoded and encoded
- **THEN** their exact values are preserved
- **AND** numeric and null window inputs remain invalid

#### Scenario: Discovery compatibility
- **WHEN** clients describe render.screenshot and its request/result types
- **THEN** operation metadata, schemas, default values and examples remain unchanged

### Requirement: Preserve screenshot validation and failure order
Editor SHALL retain the existing validation sequence: general busy state, supported window, asset-window availability, then destination validation. Standalone image preparation SHALL retain its existing destination-only validation. Failed requests SHALL preserve the current code/message and have no screenshot output side effect.

#### Scenario: Unsupported window and invalid path
- **WHEN** an otherwise drawable idle Editor receives an unsupported window and an invalid destination
- **THEN** it reports the existing unsupported-window error before destination validation

#### Scenario: Asset window absent
- **WHEN** an idle Editor has no drawable asset window and receives an assets request with an invalid destination
- **THEN** it reports the existing asset-window busy error before destination validation

#### Scenario: Pending and closing capture
- **WHEN** capture is pending or the owning window closes before completion
- **THEN** the existing busy, capture_failed and unavailable handling and pending ownership remain unchanged
