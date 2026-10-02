## Why

Asset property operations currently declare a C++ member for their value schema and a separate string for the field they read or edit. Two members with the same value type can therefore be bound to the wrong target without a type error. Deriving the canonical field from the actual reflected member removes this maintenance hazard before consolidating editing policy.

## What Changes

- Retain a safe, typed association between a registered record member and its canonical descriptor field.
- Resolve a unique member to an owned field handle without comparing pointer representations, inferring offsets or borrowing a descriptor vector element.
- Derive asset property operation targets from that association and remove independently supplied target strings from registration.
- Preserve external operation/type IDs, serialized field names, request/result wire shapes, field access permissions and existing edit workflows.
- Add same-value-type, missing/ambiguous member, descriptor lifetime and real operation compatibility coverage.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `reflected-object-archives`: Resolve an actual typed member to a unique canonical field with safe descriptor identity and lifetime.
- `automation-capability-parity`: Asset property operation schemas and target fields derive from the same reflected member association.

## Impact

Reflection record declarations and typed lookup, Automation asset property registration, focused reflection/automation tests and relevant documentation. No transport branch, native format migration, new plugin dependency or change to preview invalidation policy is included. Editing policy consolidation is a separate follow-up.
