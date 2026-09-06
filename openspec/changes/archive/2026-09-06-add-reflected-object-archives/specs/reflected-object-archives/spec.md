## ADDED Requirements

### Requirement: Automatic member binding
The engine SHALL serialize registered fields recursively without requiring per-object file IO or hand-written save/load functions.

#### Scenario: Automatic member binding acceptance
- **WHEN** a model-like object has nested records and numeric arrays
- **THEN** one generic archive call round-trips its registered persistent values

### Requirement: Versioned transactional reads
The engine SHALL reject invalid/future archives before exposing a partially constructed object and retain defaults for absent fields.

#### Scenario: Versioned transactional reads acceptance
- **WHEN** an archive is truncated or includes an incompatible field
- **THEN** loading fails and an existing caller object remains unchanged

### Requirement: Memory archive boundary
The engine SHALL encode and decode byte spans without physical file access.

#### Scenario: Memory archive boundary acceptance
- **WHEN** an archive is used with an in-memory provider
- **THEN** no file is opened by Reflection or Serialization
