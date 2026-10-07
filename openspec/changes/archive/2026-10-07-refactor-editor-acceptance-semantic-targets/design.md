## Context

`EditorAcceptanceObservations.cpp` receives `EEditorWidget` but translates ordinary widgets to string aliases in a local `FWidgetKey` table. `InspectionBounds` mixes these aliases with item IDs, wire-valued choices and concatenated reflected component/field paths. Required `.at`, optional `.contains` and polling `[]` access have different admission semantics. The same map is cleared at the current selection-inspector surface boundary.

Reparent already has typed execution phases, but its 14-case integer also chooses source nodes, selection size, root/node/outside destinations, interruption events and expected parents. Six fixtures use parallel handle/ID/world vectors with implicit roles. The cases execute sequentially, so cancelled and rejected outcomes must retain each case's initial topology rather than infer it from an ordinal range.

## Goals / Non-Goals

**Goals:** one shared typed observation-key contract across all writers/readers, explicit fixture and case identities, meaningful quantity names, unchanged event timing and coverage, and independently audited equivalent behavior.

**Non-Goals:** new editor capabilities, changed domain or automation contracts, changed fixture IDs/CLI/report keys, new test scenario registrations, unrelated numeric cleanup, archival or commits.

## Decisions

1. Keep acceptance key/storage types under Editor `Tests/Acceptance`. `FWidgetKey` identifies an existing `EEditorWidget` and an optional owned stable item ID or numeric item value. Item values remain the current authoritative wire values, not presentation indices. Single widgets need no alias string. Production observation interfaces continue to pass their existing enum/ID/value arguments.
2. Use a separate `FPropertyKey` containing the reflected component type ID and field path. Component headers are identified widgets, not fabricated properties called `header`. Own string data in stored keys. Do not invent enums for extensible reflection or asset identities.
3. A test-owned bounds store exposes required lookup, presence queries and a non-inserting empty result for polling. Migrate every `InspectionBounds` writer and consumer, including choice-action records and helper parameters. Preserve current capture admission and surface-clearing conditions. Dedicated existing bounds fields outside this map remain outside the refactor.
4. Define named Reparent cases and a single ordered definition table containing selected fixture roles, source role, discriminated root/node/outside target, interruption policy and expected outcome. The numeric case index only traverses that table; no semantic branch uses its value/range. Expected selection count derives from the selected roles. Existing phase states and transitions remain.
5. Fixture records group role, handle, stable ID and world snapshot. Preserve the six current IDs, names, transforms and order. Capture expected parents before each drag; successful delivery changes the explicitly expected selected roots, while rejected/cancelled/no-op cases preserve the captured topology. Maintain save/reopen verification using named roles and snapshots.
6. Name drag distances, transform tolerances and readiness quantities where touched. Keep literal fixture coordinates as data. Decompose changed long execution functions at preparation, preview and interruption boundaries without changing same-frame control flow.

Alternatives considered: moving the existing enum-to-string table leaves duplicate string contracts; enum-only keys cannot distinguish repeated items/properties; replacing case numbers with constants alone leaves source/target/selection/outcome relationships scattered.

## Risks / Trade-offs

- [Missing controls accidentally advance input] -> preserve required versus polling queries, returning an empty rectangle without inserting a key when polling.
- [Partial migration or mismatched item values] -> search all references, migrate shared action/helper types, compile all consumers and validate affected existing scenarios.
- [Surface clearing changes observation lifetime] -> clear both stores at the same existing boundary and preserve capture gates.
- [Case reordering changes topology expectations] -> retain current order and compare cancellation against per-case parent snapshots.
- [Refactor changes timing or coverage] -> retain input cadence/readiness/state transitions and independently compare event/assertion paths before GPU regression runs.
- [Test-owned types leak into production] -> keep new helpers in Tests and run configured source/include boundary checks.

## Migration Plan

Create the typed observation storage, migrate all consumers, introduce Reparent fixture/case definitions, then build and run affected existing Debug acceptance coverage plus relevant static checks. Use an independent reviewer on fixed hashes, independently verify findings, repair in-scope problems and re-review. Keep the OpenSpec change active and the worktree uncommitted.

## Open Questions

None for this scope.
