## Why

Existing configuration reflection only handles scalar values and string lists. Models need recursive field binding and binary payloads through the same type-driven persistence boundary.

## What Changes

- Add reflected record descriptors and typed member registration for nested objects, enums, arrays and arithmetic bulk data.
- Add versioned binary memory archives with bounds validation, defaults and transactional object construction.
- Keep legacy configuration keys and GUI reflection intact and expose memory serialization for asynchronous persistence.

## Capabilities

### New Capabilities
- `reflected-object-archives`: Existing configuration reflection only handles scalar values and string lists. Models need recursive field binding and binary payloads through the same type-driven persistence boundary.

### Modified Capabilities
None. Existing configuration, triangle and task behavior is preserved.

## Impact

Reflection and new Serialization module; Assets consumes the generic codec contract later. Depends on add-async-asset-io.
