# reflected-object-archives Specification

## Purpose
Persist registered engine objects through recursive, versioned memory archives with transactional reads and automatic field binding.
## Requirements
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

### Requirement: Memory archive boundary
The engine SHALL encode and decode byte spans without physical file access.

#### Scenario: Memory archive boundary acceptance
- **WHEN** an archive is used with an in-memory provider
- **THEN** no file is opened by Reflection or Serialization

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

### Requirement: Typed canonical member identity

Reflection SHALL resolve an actual typed C++ member to exactly one canonical field of its owning descriptor. Resolution SHALL use type-safe member equality and SHALL reject null, missing, wrong-owner and ambiguous correspondence without reading an object or comparing member-pointer representations. Existing callback-only fields SHALL remain valid without pretending to expose typed correspondence.

#### Scenario: Distinct members with the same value type
- **WHEN** two registered members have identical C++ value types and different canonical field IDs
- **THEN** resolving each actual member returns its own canonical field and cannot select the other solely by value type

#### Scenario: Missing or ambiguous correspondence
- **WHEN** lookup has no matching association, multiple matching associations, a null member or an incompatible owning C++ type
- **THEN** it fails before a caller registers or accesses a target

#### Scenario: Owned identity survives descriptor storage changes
- **WHEN** a resolved handle is retained while its source descriptor is copied, relocated or destroyed
- **THEN** its canonical identity remains valid without referring to a released member-vector element
- **AND** descriptor copies retain their definition identity while conflicting registrations or altered correspondence are rejected

#### Scenario: Existing serialized contract
- **WHEN** a descriptor gains typed correspondence without changing its declared persistent fields
- **THEN** its field IDs, aliases, versions, recursive archive bytes and wire schema remain unchanged

### Requirement: Bounded archive metadata probes

Serialization SHALL expose an in-memory current-archive metadata-range probe that returns the required prefix, directory and metadata extent from a supplied prefix and declared total. Probe and ordinary decoding SHALL share authoritative format and version interpretation, and encoding SHALL use the same format definitions. The probe SHALL validate byte availability, current magic/version, reserved values, count limits and range arithmetic before returning an extent. It SHALL NOT perform physical file access or claim that unsupplied bulk bytes have been verified.

#### Scenario: Independent known archive prefix
- **WHEN** a fixed current-format prefix with independently known lengths is probed
- **THEN** the returned extent matches the known prefix/directory/metadata boundary

#### Scenario: Malformed prefix
- **WHEN** the prefix is truncated, has unsupported magic/version or reserved values, or contains excessive/overflowing metadata or directory claims
- **THEN** probing fails before out-of-bounds access, narrowing overflow or range-driven allocation

### Requirement: Shared staged archive validation

Metadata decoding and full decoding SHALL share current archive directory and metadata structure validation, including contiguous bulk ranges, declared total, block order/index, element alignment, trailing bytes and existing budgets. Prefix probing SHALL remain distinguishable from completed metadata decoding. Full decoding SHALL retain supported legacy v1 behavior, while metadata-only probing/decoding SHALL remain current-version only. Stored bytes and canonical hashes SHALL remain unchanged.

#### Scenario: Directory boundary mutation
- **WHEN** directory entries contain gaps, overlap, reversed offsets, ranges beyond the declared total or a final extent inconsistent with that total
- **THEN** both current metadata and full decoding reject the malformed layout

#### Scenario: Metadata-only bulk values
- **WHEN** only a valid current prefix, directory and metadata are supplied with their declared full archive size
- **THEN** metadata decoding validates structure and produces null bulk values without reading/copying bulk, while preserving node/depth/allocation limits

#### Scenario: Compatible full legacy decode
- **WHEN** a previously supported legacy v1 archive is decoded in full
- **THEN** its existing values and validation behavior remain available without passing through a current-only probe

#### Scenario: Stable encoding
- **WHEN** existing scalar and bulk fixtures are encoded after the refactor
- **THEN** their independent golden bytes and canonical hashes remain unchanged
