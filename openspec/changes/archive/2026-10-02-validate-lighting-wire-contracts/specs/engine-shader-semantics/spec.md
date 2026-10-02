## ADDED Requirements

### Requirement: Validated CPU structured wire records

Direct CPU structured-buffer uploads for cluster lights, cluster headers, cluster indices and additional directional lights SHALL validate their supported physical member types, names, offsets, extents and record stride against the existing owner-declared shader contract before upload. Expected layouts SHALL remain independent of C++ storage and native reflection. Binding strides SHALL come from the validated contract. Validation SHALL require standard-layout/trivially-copyable records and SHALL NOT silently accept unsupported bool, matrix or array storage.

#### Scenario: Same-size CPU field mismatch
- **WHEN** two same-typed fields are reordered or a member's physical type/offset changes while the record's total size remains unchanged
- **THEN** CPU wire validation rejects the mismatch before publishing an upload source

#### Scenario: Header and index adapters
- **WHEN** cluster headers or indices are uploaded
- **THEN** the header's typed Offset/Count pair is validated as contiguous uint32 values at offsets 0/4 against the unnamed uint2 contract, and the index is validated against the unnamed uint contract
- **AND** their binding strides remain eight and four bytes respectively

#### Scenario: Existing lighting ABI
- **WHEN** current supported cluster and directional values are encoded
- **THEN** their 64-byte and 32-byte records, semantic IDs and contract versions remain unchanged and independent field readback agrees with fixed expected values

#### Scenario: Setup and target validation boundaries
- **WHEN** immutable lighting contracts are initialized and shader variants are prepared
- **THEN** CPU layout validation runs at setup while existing target reflection validation remains required, without per-light descriptor construction or string lookup
