# record-envelope-access Specification

## Purpose
Define Reflection-owned Record envelope construction, access and structural recognition while preserving persistence, validation and dependency traversal contracts.
## Requirements
### Requirement: Reflection owns Record envelope access
Reflection SHALL provide one shared definition for constructing and accessing the Record envelope type, version and fields. Production consumers SHALL use this contract without redefining the envelope layout. Field access SHALL retain constness and refer to the original field object without copying bulk storage or inserting absent envelope keys. Business fields such as name SHALL remain owned by their domain.

#### Scenario: Construct and edit a Record
- **WHEN** a consumer creates an envelope or edits its fields through the shared interface
- **THEN** its type/string, version/uint64 and fields/object layout, archive bytes and hash match the existing format and field edits affect the original node

#### Scenario: Missing fields
- **WHEN** a caller requests fields from an envelope without the fields key or with a non-object fields value
- **THEN** access fails without adding keys or changing the input

### Requirement: Envelope access preserves validation boundaries
Envelope access SHALL preserve existing numeric conversion, Record validation order, migration, error context and draft commit behavior. Construction SHALL NOT perform descriptor or business validation. Existing inspection paths and GUI observer IDs SHALL remain unchanged.

#### Scenario: Invalid type with malformed fields
- **WHEN** ReadRecord receives a readable version but a mismatching type and malformed fields
- **THEN** it reports the existing type/schema mismatch with the original context before attempting field binding

#### Scenario: Unvalidated inspection default
- **WHEN** inspection builds a default record value that does not yet satisfy its domain validator
- **THEN** construction succeeds and the existing commit/read boundary remains responsible for validation

### Requirement: Structural recognition preserves native dependency traversal
Reflection SHALL distinguish structural recognition from strict Record reading. Structural recognition SHALL require an object with string type, object fields and an existing version key without validating that version, registered type or domain fields. Native asset dependency traversal SHALL preserve existing reference validation and dotted versus bracketed paths.

#### Scenario: Unknown structurally recognized Record
- **WHEN** an unknown Record has a version of any node kind and contains AssetRef values in its fields
- **THEN** native dependency scanning traverses those fields with the existing dotted paths and strictly validates each AssetRef

#### Scenario: Ordinary object traversal
- **WHEN** an object does not match the structural envelope criteria
- **THEN** native dependency scanning traverses all its children with the existing bracketed paths
