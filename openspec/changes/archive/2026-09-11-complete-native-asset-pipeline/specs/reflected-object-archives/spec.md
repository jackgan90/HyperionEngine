## MODIFIED Requirements

### Requirement: Automatic member binding
The engine SHALL serialize registered persistent fields recursively without per-object file IO or hand-written save/load functions. Registered descriptors SHALL expose stable type identity, checked C++ type identity, required/default behavior, aliases and generic recursive visitation.

#### Scenario: Automatic member binding acceptance
- **WHEN** a new test asset has nested records, optional values, string-keyed maps and numeric arrays
- **THEN** registration alone enables generic round-trip persistence and visitation without modifying the asset loader

### Requirement: Versioned transactional reads
The engine SHALL apply declared adjacent schema migrations to temporary data, reject unsupported/future versions and missing migration paths, preserve optional defaults, reject absent required fields and alias conflicts, and validate before exposing or committing an object. Errors SHALL include type/field/index context; unknown fields SHALL be reported.

#### Scenario: Versioned transactional reads acceptance
- **WHEN** an archive is truncated or includes an incompatible field
- **THEN** loading fails and an existing caller object remains unchanged

#### Scenario: Migrate old nested fields
- **WHEN** an older nested record has a complete migration path
- **THEN** fields migrate before current field binding and saving emits the current version

#### Scenario: Explicit mismatch policy
- **WHEN** a required field is missing, aliases conflict, an enum is invalid or a version has no migration path
- **THEN** loading fails with contextual diagnostics without publishing partial data

## ADDED Requirements

### Requirement: Stable bounded wire format
Archives SHALL use explicit fixed tags, little-endian scalar encoding, full-range unsigned integers, indexed bulk blocks and bounded metadata/payload allocation. Legacy v1 archives SHALL remain readable for explicit upgrade. Decoding SHALL validate counts, offsets, lengths, nesting and trailing bytes.

#### Scenario: Host-independent current encoding
- **WHEN** known scalar and bulk values are encoded
- **THEN** golden bytes match the fixed protocol and damaged ranges or exhausted budgets are rejected

### Requirement: Registered type discovery
A registry SHALL map stable IDs to checked C++ descriptors and reject conflicting registrations.

#### Scenario: Untyped asset load
- **WHEN** a registered root type is discovered from a file
- **THEN** it is constructed by the matching descriptor and an incompatible typed cast fails
