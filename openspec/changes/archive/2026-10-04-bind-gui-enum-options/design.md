## Context

Baseline `05d572c` has material sampler label arrays cast to enum ordinals. FPropertyPresentation::Choices stores labels only; both Gui inspectors and Reflection inspection validation interpret the array position as the scalar value. Three production declarations use this representation (directional shadow, contact shadow and environment source).

## Goals / Non-Goals

Goals: explicit value/label association, identical behavior for ordinary and mixed inspection, existing sampler labels and enum values, unchanged validation/transactions/persistence.

Non-goals: new controls, changing automation schemas, redesigning all Editor options or moving domain validation into Gui.

## Decisions

- Replace positional inspector choices with FPropertyChoice records containing an archive value and label. Archive values preserve the field's signedness/type and avoid restricting enum representation. Reuse EqualInspectionValue for matching and expose shared index/label helpers. Reject unknown choices during inspection validation, not by label count.
- Migrate all three current declarations together; do not keep two alternative choice inventories. The shadow change supplies its final canonical descriptions; environment-source values remain explicit typed enum values.
- Sampler controls derive values from RecordEnumEntries, with explicit presentation label overrides for existing spaced labels. Editor owns these display overrides; Materials remains the authority for supported values. The widget selects the entry's value, never casts its index.
- Material type labels use explicit enum mapping. The existing editing service still validates and commits changes.
- Test reordered/sparse choices, mixed selection and unknown values; retain existing archive and GUI regressions. This intentionally extends internal choice representation only, not reflection persistence.

## Risks / Trade-offs

- Signed/unsigned or numeric archive mismatch -> choices use WriteValue of the actual field type; test typed archive values and existing integer validation.
- Mixed widgets may diverge from ordinary widgets -> share lookup helpers and exercise actual GUI selection in both paths.
- Display spelling changes -> freeze existing sampler labels separately from enum protocol names.

## Migration Plan

Migrate declarations and consumers atomically, run Debug/Release regressions, and leave the active change unarchived and uncommitted. No data migration is required.
