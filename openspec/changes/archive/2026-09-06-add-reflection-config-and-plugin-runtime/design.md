## Context
Core services and execution domains are available. Configuration must be independent of any renderer or JSON library, and plugin startup must be deterministic.
## Goals / Non-Goals
**Goals:** reflected scalar/string-list fields, default handling, range validation, schema identity/version, transactional loading and static plugin dependency resolution.
**Non-Goals:** AST generation, GC, binary ABI compatibility or dynamic module reload.
## Decisions
- Represent reflected values using engine-owned variants and property access functions, keeping nlohmann/json private.
- Save type ID, schema version and named properties. Reject unknown future versions and invalid field types/ranges. Missing fields keep the caller's defaults; validate all values before applying any.
- Write to a sibling temporary file, then replace the target using the platform's replace operation to protect existing configurations from partial writes.
- Resolve requested plugins through a depth-first dependency traversal with cycle and missing-ID detection. Start in dependency order and stop in reverse order; roll back successful starts if a later startup fails.
## Risks / Trade-offs
- Field renames change persistence → preserve field IDs or introduce explicit migrations in a future change.
- Arbitrary setter exceptions → setters provided by the reflection registration must use validated values; built-in settings are loaded into a copy before replacement.
