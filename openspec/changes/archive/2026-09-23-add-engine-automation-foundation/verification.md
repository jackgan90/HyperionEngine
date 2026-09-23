# Verification

Validated on 2026-09-23 in the working tree based on `7c711cb1c261e72b7dbf26ef9abcca28b56316e4`.
No commit or OpenSpec archive was created.

## Build and checks

- Full Ninja/MSVC x64 Debug and Release builds passed, including Editor, Viewer, the new automation host and all existing test targets. Final incremental builds included the framing correction and current sources.
- Both configured profiles used Triangle/ModelViewer/SceneViewer/DebugUI/RenderDoc ON and Tracy OFF. This records the actual tested profiles, not a new required project configuration.
- `python tools/CheckStyle.py`: filename/include case and full owned-source formatting passed.
- The repository's `CheckStyle.check_naming` checks passed for all 25 changed/new C++ translation units against the Debug compilation database. The final stdio framing edit also passed an incremental clang-tidy/clang-query check.
- `python tools/CheckBoundaries.py`: passed, including CPU-only transitive constraints for Automation and AssetEditing.
- Updated documentation relative links and `git diff --check`: passed.
- `openspec validate add-engine-automation-foundation --strict`: passed.

## Regression selection

The following command passed **18/18 tests in Debug and 18/18 in Release** (including the automatically required model fixture):

```powershell
ctest --test-dir out/build/debug -R '^automation_|^editor_asset_|^async_io$|^reflected_archives$|^material_contracts$|^scene_management$|^native_asset_(management|registry|publication)$|^configuration_plugins$|^plugin_runtime$|^material_asset_contracts$|^shared_asset_publication$' --parallel 4 --output-on-failure
# Same selection with --test-dir out/build/release
```

The final per-line framing correction was followed by a Debug `^automation_` rerun (3/3) and is included in the Release selection above. This was a targeted regression run, not a claim that the entire CTest suite was executed.

New tests cover:

- Strict reflected natural JSON, required/unknown fields, enum/fixed-array/numeric validation, 64-bit integer preservation, parser budgets, lazy schema and registration extension.
- Main ownership, duplicate/late registration rejection, unavailable providers, bounded deferred jobs, cancellation and stopped admission.
- MCP initialization, notifications without replies, stable bootstrap list and typed invocation.
- GUI-shared draft/history behavior; saving a captured revision while later edits remain dirty; external disk conflicts; stale/closed handles; busy saves/edits and dirty close.
- Real CLI, persistent JSONL and MCP processes; native persistence/reopen, UTF-8 names, read-only mounts, disabled/missing/failed providers, session isolation and EOF with pending saves.
- Input from pipes and regular files, oversized input rejection, and independent message budgets when one pipe read crosses a near-limit line boundary.
- Scoped operation withdrawal after injected adapter startup failure and orderly provider shutdown without closing an externally owned asset service.

Existing `editor_asset_documents`, `editor_asset_workspace` and `editor_asset_editors` passed in both profiles. The latter two exercised actual D3D12 asset previews/editor actions with the repository's isolated fixtures and hidden-window acceptance path.

## Quality audit follow-up (2026-09-23)

An independent reviewer audited the combined uncommitted automation/content-root work against baseline `7c711cb1c261e72b7dbf26ef9abcca28b56316e4`. The parent independently confirmed the two P2 findings before making scoped repairs. No commit or archive was created.

- QA-01: separate bounded response framing from request/domain-result budgets; preserve JSONL correlation and continuation; distinguish post-execution `result_unavailable` from rejected arguments. Regressions exercise 600 KB names, escaped names, deferred results, near-1 MiB input/result expansion, and long IDs through both JSONL and MCP. Synchronous/asynchronous result failures are checked at the session layer.
- QA-02, in the companion content-root change: validate actual library subtrees instead of accepting an empty selection after physical-to-package normalization.
- Full Debug and Release incremental builds passed. The selected `automation_contracts`, `automation_assets`, `automation_transport`, `editor_asset_documents`, `native_publication_cli`, and `reflected_archives` tests plus required `model_fixtures` passed: all 7 distinct tests in Debug and 7/7 in Release. Debug initially encountered duplicate IDs in a newly added test fixture; isolating the fixture yielded a passing `native_publication_cli`/fixture rerun (2/2).
- Full source style and dependency checks passed; semantic naming passed for all 6 C++ translation units touched by audit repairs. Both strict OpenSpec checks and whitespace checks passed.
- Raw audit evidence, before/after snapshots and repair logs are under `out/QualityAudit/AutomationContentRoot-20260923-111809`. Existing GPU validation predates these CPU/transport repairs; this audit round did not rerun GPU or the full CTest suite.

## Delivery boundary

This change establishes the shared contract and first asset-document adapter. Scene editing, arbitrary property/reference editing, renderer/GPU operations, import/publication catalog integration and attachment to an already running Editor remain explicitly deferred in `docs/Automation.md`. No existing executable name, asset format or native backend interface was changed. Editor consumes the extracted AssetEditing implementation.

Local generated logs are under `out/Automation*Build.log`, `out/AutomationNaming.log` and each build directory's `Testing/Temporary`; they are not versioned artifacts. Source, documentation and OpenSpec files remain uncommitted for review.

## Archive handoff (2026-09-23)

The statements above record the implementation and audit stages before archive authorization. The original independent reviewer subsequently verified the repaired snapshot, closed QA-01 and QA-02, and passed 6/6 targeted CPU tests, including the required fixture. The user then authorized archive and Git commit. This change was archived with its main specifications synchronized alongside [the content-root change](../2026-09-23-replace-content-mount-config/verification.md); no engine code changed during archive. Generated audit evidence remains outside the committed file set.
