## Context

`FBulkData::Element` currently carries a free string. Serialization separately validates ten spellings and derives width by parsing digits; GUI, AssetImport and Reflection wire code each repeat type lists. `BulkElement<T>()` constructs names from arithmetic category and width. Scene clipboard budgeting also counts the stored name. All consumers already depend on Reflection.

## Goals / Non-Goals

**Goals:** Make supported bulk identity and dispatch authoritative in Reflection; preserve the ten existing wire tokens, binary archive bytes and hashes, source JSON shape, wire numeric rules, array validation and shared storage ownership.

**Non-Goals:** New numeric types, reflection fields or registration, archive versions, record envelope APIs, plugin composition, automation operations or UI behavior changes. This stage does not combine other roadmap items.

## Decisions

1. Add a compact `BulkElement.h` contract. One scoped macro declaration lists identity, C++ primitive and wire token; it generates the enum, metadata and typed visitor, and is undefined at the end of the header. Width and signed/floating category derive from the primitive. This avoids an enum plus independently maintained switch/table. Enumeration values are internal and never serialized.
2. Store `EBulkElement` in `FBulkData`. A default/invalid identity has no metadata and is rejected at persistence/dispatch boundaries. Name parsing is exact and returns an optional. `BulkElement<T>()` maps category and width through the same metadata, retaining compatible arithmetic aliases and `std::byte` as u8. Unsupported widths fail explicitly.
3. Serialization converts names only at its read/write boundary, preserving existing unknown-token and alignment rejection, v1/v2 layouts, allocation charging and owned/shared views. GUI and JSON export use the typed visitor. Reflection wire unpack uses the visitor; packing selects metadata by its explicit numeric category and byte width, retaining numeric range and finite checks.
4. Scene clipboard continues accounting for archive-node memory and encoded element-name bytes using the shared metadata. The existing budget policy remains; actual `sizeof` accounting naturally reflects the smaller in-memory node representation. No resource ownership or lifetime changes accompany this representation refactor.
5. Keep domain validation in its existing consumers. The common visitor only selects a C++ primitive; it does not edit properties, encode JSON, interpret wire values or own archive storage. Unknown GUI elements continue to produce no edit; archive/wire/export boundaries fail explicitly.

## Risks / Trade-offs

- C++ callers constructing bulk data with strings must migrate together → build repository consumers and Editor/Automation executables; no serialized migration is needed.
- A wrong common mapping affects every consumer → independent ten-type fixtures specify names, widths, categories, byte payloads and expected JSON/wire values separately from the production table; existing known archive bytes/hash fixtures remain unchanged.
- Narrow integers, u64/i64 and floating values use different wire rules → test extrema, 64-bit string projections, finite checks, empty sequences, misalignment and type mismatches.
- Header templates have macro/naming exposure → use a local declaration macro, undefine it, manually review it and run semantic naming checks on instantiated consumers.

## Migration Plan

Capture baseline validation, introduce the contract and migrate all consumers, run compatibility and focused integration tests, then independently audit a frozen snapshot. Repair confirmed findings and obtain targeted re-review. Stop for user diff acceptance before archive or commit. The temporary planning document remains untracked and excluded.

## Open Questions

None for this stage.
