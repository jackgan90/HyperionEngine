## ADDED Requirements

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
