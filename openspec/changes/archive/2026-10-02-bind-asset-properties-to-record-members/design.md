## Context

`Member(Id, Pointer, Options)` already captures the actual pointer-to-member in Reflection callbacks, but `FRecordMember` exposes no typed correspondence. `RegisterField<Member>` derives a schema from the template member and separately accepts `InField` for `ReadField/SetField`. Radiance, Specular and Brdf demonstrate a same-value-type mismatch that schema validation cannot detect.

Record descriptors retain shared definition identity across copies and the registry compares callbacks and metadata to reject conflicts. Operation callbacks outlive registration and must not borrow a member-vector element. Record declarations include custom callbacks, so typed lookup cannot require every legacy field to have a direct C++ member.

## Goals / Non-Goals

**Goals:** Derive a unique canonical field from an actual member; keep that identity safe across descriptor copies/lifetimes; use it for typed asset property registration; prove wire and operation compatibility.

**Non-Goals:** Editing policy or preview invalidation changes, a new reflection system, mandatory migration of custom callback fields, operation renaming, transport changes, disk migrations, arbitrary member-offset access and plugin lifetime redesign.

## Decisions

### Reflection owns typed member correspondence

Extend the existing `Member` declaration path with an optional typed association. Compare pointer-to-member values with C++ equality only after checking their exact member-pointer type; never use memcmp, integer conversion, guessed offsets, object addresses or default field values. A small owned type-erased association can hold this operation and the member-pointer value. It does not know about assets, documents, GPU work or automation.

Typed resolution checks the requested owning C++ type against the descriptor, searches for the actual member and rejects null, missing or ambiguous correspondence. Two different members of the same value type resolve to distinct canonical IDs. Custom callback-only fields continue to serialize and register normally, but cannot claim typed correspondence they do not provide. An aliased wire spelling is not a second member identity.

Use the existing descriptor declaration as the authority rather than an additional manually maintained member-to-name table. Hard-coded enum identities or a second field string would preserve the original mismatch risk.

### Return an owned handle with canonical identity

The lookup result owns the canonical type/field names and the identity/callback data it needs; it must not hold a borrowed pointer into `Members`. Tie it to the descriptor's existing shared definition identity. Descriptor copies retain correspondence, and the handle remains usable after the source descriptor/vector is released or relocated. A separately constructed incompatible descriptor with matching names cannot silently become the same registered contract.

If the API accepts a handle together with a descriptor, verify the definition and field association before use. If registration only needs stable names, capture the resolved owned identity by value. Do not invent a process-global member registry. Preserve all existing descriptor conflict checks and include the new association when comparing copies for registration; copied descriptors remain accepted, altered correspondence remains rejected. This must not impose a new restriction on otherwise valid custom callback descriptors.

### Operation registration has one target source

`RegisterField<Member>` resolves its canonical target from `RecordType<FSource>()` and removes its independent `InField` parameter. Request/result schemas continue to derive from the same template value type. The operation ID remains explicitly supplied because external task naming differs from persisted field naming (`model.material_slots` versus `materialSlots`). Capture owned resolved identity for get/set and partial sequence replacement.

Keep all current operation IDs, schema/type IDs, versions, pagination and offsets, read-only flags, provider availability, generation checks and asynchronous field workflow unchanged. No operation becomes writable merely because typed lookup succeeds. Existing string APIs in AssetEditing may receive the resolved canonical ID during this step; consolidating their policy is the separate M11B change.

### Preserve contract and lifecycle evidence

Before migration record current discovery IDs/versions and request/result schemas for the affected operations. Test with independent expected IDs and values; a helper resolving the field under test cannot be the sole oracle. Give same-typed fixture fields distinct values and prove each real get operation returns its own field. Exercise writable sequence/value operations, pagination/offset replacement, read-only set absence, stale rejection and existing workflow drain/document-replacement tests.

Reflection tests cover same-type fields, missing/null/ambiguous members, wrong owning type, custom callback descriptors, copied descriptors and source lifetime, and altered registration conflicts. Existing recursive persistence, aliases, migrations and fixed archive byte tests must remain unchanged. A typed handle does not alter serialized IDs, field order or schema.

Use Debug/Release `archive_tests`, `automation_tests` and `automation_asset_tests` plus relevant existing editor asset document/workspace consumers. Actual CLI/MCP discovery and invocation must be exercised through existing parity regression harnesses with rebuilt executables where required. If application discovery needs an isolated test directory, restrict it to child-process environment and retain the reason; do not alter user discovery records.

## Risks / Trade-offs

- [Pointer representation is implementation-specific] → Only exact typed equality; no representation-based hashing or offsets.
- [Descriptor lifetime or conflict checks weaken] → Owned handle plus existing definition identity and explicit copy/conflict tests.
- [Custom fields become invalid] → Typed association is optional; require it only for typed lookup.
- [Schema stays correct while target changes] → Remove the second target parameter and test real same-type get outputs with distinct values.
- [Scope expands into edit effects] → Keep all policy/preview behavior unchanged and defer M11B as a separate accepted step.

## Migration Plan

Capture current operation contracts and reflection regression baseline; add typed correspondence/lookup; migrate only asset property registration; rebuild and run affected reflection, automation and consumer tests; update documentation; obtain independent review. This is source-only migration with no stored-data upgrade. A scoped revert restores the previous registration path.

## Open Questions

None blocking. Exact association/handle names are implementation choices within the typed-equality, ownership and compatibility requirements.
