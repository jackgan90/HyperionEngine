## Context

`FSceneLightDiagnostic::Type` is an open string field with an empty default. Renderer produces `directional` or `sky`; Editor compares those tokens and the `priority`/`sky` field IDs. Reflection accepts arbitrary strings, including empty and embedded NUL values. A closed enum projection would silently narrow that existing contract.

## Goals / Non-Goals

**Goals:** Use typed identity in diagnostic production and Inspector routing; preserve all wire/archive values, schema, defaults, lighting selection and presentation.

**Non-Goals:** Lighting algorithms, sky preparation state, screenshot targets, document transition state, plugin composition, and new automation operations.

## Decisions

1. Renderer owns `ESceneLightDiagnosticKind` and `FSceneLightDiagnosticType`. The value stores a variant of a known enum or an opaque string, with one enum/token table. Native production constructs from the enum. Wire decoding recognizes the two known tokens and retains all other strings verbatim. The default remains an opaque empty string. A closed enum would break historical inputs; retaining a public string plus a derived enum would continue allowing native spelling mistakes or duplicated state.
2. The reflected `type` member explicitly projects the value as a string with the same ID, default, version, persistence and member association. No enum schema is published. Opaque values have no known kind and cannot match a known native diagnostic. This is a local compatibility adapter, not a new generic Reflection facility.
3. Editor resolves incoming field IDs against the actual component record and matches C++ member associations for directional/environment Priority and environment Sky. A small private light-inspection helper owns this classification and existing presentation logic. The plugin obtains the shared diagnostic snapshot only after a supported property resolves, preserving the current early exit. Unit tests can exercise presentation and renamed descriptor fields without a GPU.
4. `scene.lighting.get` and GUI continue consuming the same Renderer snapshot. No transport branching, service lifecycle or dependency direction changes are needed.

## Risks / Trade-offs

- Open string compatibility adds a small tagged value → keep its storage private, reject invalid native enum construction, and test known/opaque transitions and full wire/archive round trips.
- Custom reflection projection could change discovery → compare actual operation/type descriptions before and after and assert string shape, default and native association.
- Moving Inspector presentation could change messages or branch ordering → retain existing decisions and text, test disabled/selected/overridden/tied and sky failure/loading/ready cases, plus wrong handles/kinds and renamed member IDs.

## Migration Plan

Capture contracts, implement the local types and consumers, run focused native/GPU/automation checks, then independent audit. Stop for user diff acceptance before archive and commit. No persisted-data migration; rollback restores the former native representation.
