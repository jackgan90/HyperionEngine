## Context

M11A establishes safe member-to-canonical-field identity. The remaining confirmed M11B paths are `AssetEditing/AssetProperties.cpp` field-name preparation branches, `AssetDocument.cpp::Set` caller-supplied preview flag and name special case, `ModelProperties.cpp` primitive effects, Editor's primitives string branch and node-name false flag, and `AssetEditWorkflow.cpp` prepared reference completion.

Current direct GUI node renames pass `false`, while generic automation `model.nodes.set` uses the default `true`. Both edit the same persisted field. The central policy intentionally removes this conservative invalidation discrepancy: node names are metadata regardless of caller; Local changes affect previews. This is a bounded behavioral correction in invalidation, not a new editing capability.

## Goals / Non-Goals

**Goals:** Canonical field identity owns access, validation/normalization, reference preparation and before/after effects in AssetEditing; all adapters use it; existing history, persistence, bulk sharing and async lifetime remain valid.

**Non-Goals:** Generic property UI generation, changing asset schemas/operation IDs, adding writable fields, moving asset policy into Reflection, renderer/GPU work in the CPU domain, replacing the archive draft/history model, or new plugin services/transports.

## Decisions

### Policy belongs to AssetEditing and uses M11A identity

Use a small focused policy inventory for the currently supported Model/Material/Texture/Sky editor fields. Each association is derived from an actual typed reflected member through M11A, not a separately repeated field string. Match the descriptor definition/type and canonical field safely; never compare member offsets or retain descriptor-vector pointers. A policy can expose its edit route (ordinary field, dedicated primitive/encoding operation, read-only), preparation and change-effect computation. Concrete names and static table layout are implementation choices; avoid a new generic registration framework.

Reflection remains unaware of documents, tasks, asset references, history or previews. Canonical persisted strings remain valid at API/history/archive boundaries, but are resolved to a policy once rather than repeatedly branching in GUI and automation. External operation/type IDs and request/result schemas remain exact. Generic Set availability derives from policy access; dedicated primitive/texture APIs keep their current public entrypoints and protections. A read operation does not imply a generic write operation.

### One preparation path, domain-owned effects

Refactor existing preparation algorithms into field policies rather than rewriting their semantics. Preparation receives current document state plus candidate value, validates complete candidate shape and invariants, normalizes permitted values, and returns owned candidate/reference requirements and derived effects. Existing material active/source/override constraints, finite/clamped numeric rules, texture dimensions, node identity/topology and material-slot count/type checks remain.

Remove the public caller choice `bAffectsPreview` from document/commit/editor APIs. The domain computes effects from actual before/after values at a validated commit boundary; a caller cannot forge or override them. An internal low-level apply function may remain for already prepared domain operations, but must not be exposed as an adapter-controlled bypass. Keep the implementation small: reuse existing preparation and document history, with encapsulation or recomputation sufficient to keep effects authoritative. Do not add a new transaction framework.

The fixed behavior matrix is:

| Edit | Persisted/history state | Preview effect |
| --- | --- | --- |
| Supported asset name | Existing transaction semantics | None |
| Model node names only | Existing node identity/topology retained | None |
| Model node Local changes | Existing node validation | Invalidate |
| Primitive names only | Existing identity/geometry retained | None |
| Primitive material assignment changes | Valid existing slot or -1 | Invalidate |
| Existing material-slot references | Existing count/type/graph checks | Invalidate when value changes |
| Permitted material values | Existing normalize/reference rules | Invalidate when effective stored values change |
| Texture encoding rebuild | Dedicated full-root candidate, mip0/shared bulk preserved | Invalidate for the committed encoding/payload change |

Preview effects are based on normalized committed values, not a single constant per top-level field. Mixed name+transform edits invalidate. Field policy does not independently change generation/history behavior for equal-value submissions; preserve existing admission and interaction semantics unless a directly verified invariant requires adjustment. Coalesced history ORs actual preview effects across its edits, preserving Undo/Redo/Cancel behavior even if a drag returns to its starting value. History stores canonical target identity and the domain-computed effect at execution time, not a borrowed policy pointer or recomputation under a later definition.

### Keep dedicated operations and asynchronous admission

Primitive editing continues to protect stable ID, geometry counts/content, topology and material range. GUI and automation feed the same dedicated policy/service; GUI no longer branches on the literal `primitives`. A finite typed route returned by the domain or a shared domain dispatcher is sufficient. Generic field setters must not open raw primitive geometry replacement.

Texture encoding remains a distinct asynchronous operation producing a complete draft. Do not model an arbitrary empty field string as a new public generic setter; keep root replacement internal to the domain and preserve the existing exact mip/encoding history and bulk sharing. Float/cube encoding stays read-only.

Reference requirements are collected by the same policy for GUI, standalone and attached workflows. Existing `FAssetEditWorkflow` joins loads, validates graphs and rechecks document pointer, native AssetId and generation before commit. Preserve these guards and ensure a prepared mutation is committed only against its matching state. Drain/destruction waits and releases admission without applying a result. Do not add a new direct-commit reference path that skips validation currently performed by the workflow. The change does not broaden support for editing unknown/custom asset formats.

### Adapters choose values, not policy

Editor widgets keep their presentation and interaction tracking but identify their target through canonical typed identity, call the shared domain path, and use policy access/route as needed. Automation typed member registrations retain their external IDs, pagination and wire records from M11A; writable availability and field handling come from the same policy. Material numeric, model primitive and reference adapters must consume the shared effects instead of retaining private preview decisions. No branches are added to transports.

## Verification

After M11A acceptance, capture the current full property API snapshot and baseline existing document/workspace/workflow tests before production edits. Baseline must record the existing automation-node-name conservative preview increment as the explicit behavior being corrected; do not label the future no-increment expectation as passing old code.

Run the same name-only, mixed Local, primitive name/material, numeric and texture-reference edits through GUI-domain entrypoints, standalone automation and attached provider/workspace paths. Compare normalized draft, generation/dirty/save state, history and PreviewGeneration. Include Undo/Redo/Cancel and coalesced interactions, equal/normalized-equal candidates, save during edit and root replacement. Assert read-only fields remain absent from generic Set discovery and direct domain rejection leaves the document unchanged. Compare full schemas to the accepted M11A snapshot.

Retain wrong type/dimension/graph, stale generation, replaced/closed document and Drain-without-commit tests. Preserve top-level texture bulk storage sharing and exact mip history. Build actual existing targets including archive_tests, automation_tests, automation_asset_tests, editor_asset_document_tests, editor_asset_workspace_tests, hyperion_editor and hyperion_automation_cli; resolve current CTest names from CMake and run relevant document/editor/workspace/automation parity/transport tests in Debug/Release. GUI automation acceptance must execute the same paths whose equivalence is asserted; a compile-only claim is insufficient.

## Risks / Trade-offs

- Policy table duplicates persisted identities → derive every supported association from M11A typed member lookup and reject unresolved/foreign definition identity.
- Caller-controlled effects survive behind a new wrapper → remove adapter flags and review the final document apply boundary, not only registration syntax.
- Whole-field effect hides mixed edits → compare node Local and primitive Material independently of names and test mixed candidates.
- Prepared edits outlive their source state → preserve generation/document checks and owned values; use no borrowed descriptors or temporary pointers.
- Tightening generic access accidentally changes existing operation exposure → full schema/discovery and dedicated-operation parity tests before/after.

## Migration Plan

Accept M11A first; add and run baseline evidence; introduce policies with current preparation; migrate document/history effects and dedicated operations; migrate GUI/automation callers; validate compatibility and the explicit name-only invalidation correction; then independent frozen review. No disk migration is required. M11 remains incomplete until both A and B pass their separate acceptance.

## Open Questions

None for scope. Implementer selects minimal internal visibility and prepared-value ownership consistent with the authoritative effect and existing async guards; expanding into generic transactions is outside scope.
