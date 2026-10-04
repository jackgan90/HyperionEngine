## ADDED Requirements

### Requirement: Authoritative typed bulk element definition

Reflection SHALL own one declaration of the supported bulk identities, C++ primitive types and existing wire names. Metadata SHALL derive widths and numeric categories from those types. Typed dispatch and parsing SHALL use that same definition. Bulk archive nodes SHALL carry typed identity; enum ordinals SHALL NOT become persisted values.

#### Scenario: Supported element mappings
- **WHEN** any of u8, i8, u16, i16, u32, i32, u64, i64, f32 or f64 is parsed or dispatched
- **THEN** it selects the existing primitive, signed/floating category and byte width, including compatible C++ aliases and std::byte as u8

#### Scenario: Unknown identity
- **WHEN** an unknown name or invalid enum identity reaches archive decoding, encoding or shared dispatch
- **THEN** it is rejected explicitly without selecting another element type

### Requirement: Preserve archive and storage compatibility

Serialization SHALL retain existing bulk names, archive v1/v2 layouts, encoded bytes and hashes, alignment checks, metadata-only behavior, budgets and storage ownership. Typed sequence reads SHALL retain mismatch, fixed-array length and non-finite rejection.

#### Scenario: Known data and retained views
- **WHEN** known bulk values are encoded or a current or legacy archive is read with or without shared byte storage
- **THEN** values and bytes match the existing format and shared views retain their original owner and range

#### Scenario: Malformed and empty payloads
- **WHEN** a payload is empty, misaligned, non-finite or read through an incompatible element type
- **THEN** empty supported sequences succeed and invalid inputs follow the existing rejection rules

### Requirement: Shared consumer dispatch preserves projections

GUI editing, asset source JSON export and Reflection wire conversion SHALL select primitives through the common bulk contract. They SHALL preserve supported values, GUI editing behavior, the existing $bulk/data source JSON representation and wire numeric rules, including decimal-string representation of 64-bit integers. Scene clipboard SHALL continue its authored-data budget policy using the typed element metadata for encoded-name accounting.

#### Scenario: Numeric projection and round trip
- **WHEN** each supported bulk type is exported to source JSON or projected to and from its reflected wire shape
- **THEN** the existing element names, numeric values, widths and 64-bit string rules remain unchanged without duplicated consumer type lists

#### Scenario: GUI element editing
- **WHEN** an editable numeric bulk element is visited by the property inspector
- **THEN** the existing scalar editor and read/write validation run for the same primitive type and commit only actual edits
