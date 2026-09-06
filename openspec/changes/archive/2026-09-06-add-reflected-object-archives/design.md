## Context

Existing configuration reflection only handles scalar values and string lists. Models need recursive field binding and binary payloads through the same type-driven persistence boundary.

## Goals / Non-Goals

Goals: Add reflected record descriptors and typed member registration for nested objects, enums, arrays and arithmetic bulk data. Add versioned binary memory archives with bounds validation, defaults and transactional object construction. Keep legacy configuration keys and GUI reflection intact and expose memory serialization for asynchronous persistence.

Non-goals: no implementation of future FBX/OBJ/COLLADA codecs, animation, compressed glTF extensions, full asset cooking, ECS or additional graphics backends in this series.

## Decisions

### 1. Decision

Keep FTypeDescriptor/FProperty for current GUI/configuration compatibility. Add FRecordDescriptor and typed Member registration with generic recursively inferred codecs. No AST generator or mandatory base class.

### 2. Decision

Represent archive fields in an engine-owned value tree; member adapters traverse nested records, arrays, enums, strings and arithmetic bulk buffers. The binary format carries magic, type ID, schema version and bounded named values; bulk arrays retain element type and byte count. Never persist object addresses or GPU resources.

### 3. Decision

Decode into a fresh default-constructed object, validate types and ranges, and only publish on success. Unknown fields are skipped, missing fields retain defaults, future versions fail. Runtime record IDs are stable serialized contracts.

### 4. Decision

Serialization consumes memory; IO service owns disk scheduling. Legacy JSON gains memory encode/decode while retaining current schema and transactional validation. Native archive save/load is symmetric; external format export is an explicit codec capability.

## Risks / Trade-offs

Malformed nested data can exhaust resources; cap byte size, depth and container counts and check arithmetic before allocation. Binary bulk representations specify little-endian arithmetic rather than C++ struct layout.

## Migration Plan

Implement after `add-async-asset-io`. Keep existing target names, serialized keys, triangle and configuration tests working. Each change is additive until its replacement paths are verified; retain explicit compatibility wrappers where required. Validate focused tests before continuing and run full Debug/Release verification at the end.

## Open Questions

No blocking product decisions. Implementation refinements must be reflected here and validated before task completion.
