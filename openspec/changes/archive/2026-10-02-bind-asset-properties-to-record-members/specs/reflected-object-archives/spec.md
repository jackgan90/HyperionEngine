## ADDED Requirements

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
