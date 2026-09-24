# Quality audit — 2026-09-24

## Scope and review method

Reviewed all uncommitted tracked changes and new Source/OpenSpec files for this change against `4d93861b374fbf7103f889b8f4b288f816db6819`. There were no staged changes. Pre-existing untracked `editor-asset-tests/` and `shader-fixtures/` were excluded and preserved.

A fresh independent reviewer with no inherited conversation context read the full change and its direct call chains. Reviewed files were frozen during both audit rounds. The main agent independently reproduced the finding, applied the minimal repair, and requested targeted re-review from the same reviewer.

Generated snapshot manifests under `out/AutomationDiscoverabilityAudit/` identify the reviewed versions:

- InitialSnapshot.txt: 65 files; manifest SHA256 `3FF2DFA10301DCAB3C5CF7F1DF0C8491DC97AE90B7F0795C3A62E725806462D7`.
- ReReviewSnapshot.txt: 65 files; manifest SHA256 `F7EA312520C57803A778B28E26B061A580544F35E8D943C6F5C0DEE03DC645C3`.

This audit note and the final validation addendum were written after re-review; reviewed source files did not change afterward.

## AD-01 — P2 — confirmed, repaired, re-reviewed

`Runtime/Reflection/Private/WireSchema.cpp` generated `oneOf` from labels rather than the complete legal enum values. A legacy `RecordEnumValues` containing values 1 and 2, with a label only for 1, produced `enum:[1,2]` but `oneOf:[{const:1}]`. Wire decoding still accepted 2, and a default of 2 violated its own schema. Duplicate numeric aliases similarly produced multiple matching `oneOf` branches.

Both agents confirmed the partial-label case using an isolated C++ reproducer under `out`. This defect was introduced by this change's reusable metadata mechanism; the current complete production enum tables did not trigger it.

The repair builds schema alternatives from distinct encoded legal values. Unlabelled values retain a `const` branch; labels and descriptions for aliases are combined into the same branch. Wire validation, persisted values and transport behavior remain unchanged. Regression coverage verifies partial labels, duplicate aliases, uint64 maximum/default encoding, exactly one branch per accepted value, wire roundtrips and rejection of an illegal value.

The original independent reviewer verified the updated snapshot, independently ran the rebuilt automation contract executable and closed AD-01 with no new findings. The main agent also reran the original reproducer against the repaired library: value 2 now has a schema branch and remains accepted by wire decoding.

## Compatibility boundary

The new client accepts old targets without mode/label. An old client strictly decodes target records and cannot discover or connect to a new host that emits these fields. Bidirectional mixed-binary compatibility was not an acceptance requirement of this change. `docs/AutomationConnections.md` now explicitly requires updating/restarting CLI/MCP when updating the host and clarifies that protocol number 1 does not guarantee mixed-version compatibility. No protocol redesign was added.

## Validation and limits

- Debug build succeeded; `BuildFinal.log` contains no MSVC compiler/linker warnings or errors.
- All 11 selected tests passed after repair in 156.43 seconds: automation_assets, automation_contracts, automation_connections, automation_transport, reflected_archives, material_contracts, material_bindings, automation_attachment, automation_capability_parity, automation_parity_regressions and material_asset_contracts. Evidence: `FinalTests.log`.
- Whole-repository path/format checks passed (888 files); module boundary check passed (852 files, 42 modules); semantic naming checks passed for both repair translation units (`NamingVerified.log`).
- OpenSpec strict validation and `git diff --check` passed.
- Fresh Debug CLI/MCP and hidden Editor/Viewer integration tests supplied runtime evidence. The existing Release MCP processes were not used to validate new behavior or replaced. No Release rebuild or whole CTest suite was performed in this audit.

No unresolved in-scope findings remain. The OpenSpec change remains active and uncommitted; no staging, commit or push was performed.
