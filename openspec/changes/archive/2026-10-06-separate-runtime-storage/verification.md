# Development verification

Date: 2026-10-06. Implementation baseline: `e471e8211f9ce4bc9b7515b2ccc7ba9a487eea9d` on `main`. Development validation preceded user acceptance; the user subsequently accepted the implementation and authorized OpenSpec archive/spec synchronization followed by a local Git commit. Push is outside the authorized delivery scope.

## Delivered scope

- IO application/profile paths and native directory lookup; Config owns revisioned bootstrap settings, override precedence, frozen active roots, non-destructive Config/State relocation and one-time known legacy import.
- Independent local DerivedDataCache with semantic Shader keys, record integrity, atomic publication, recoverable storage failures and bounded cross-batch maintenance.
- Editor preferences and reflected `application.storage.get/set` share Config validation and persistence. Optional providers and adapters retain scoped cleanup. Discovery uses the stable locator-relative namespace, with an explicit environment override.
- Independent locked developer ToolCache, alternate build/output roots, per-run acceptance storage and inherited child-process isolation. Current project documentation is updated without importing historical evidence.

## Executed checks

| Check | Result |
|---|---|
| Full Debug build | Passed, including Editor, CLI, AssetTool and tests |
| Debug CTest | All 160 registered tests covered. Initial run: 157 passed and 3 harness failures. After fixing the harnesses, all 9 selected regression tests passed, including all 3 previously failing tests |
| Final focused tests | `automation_transport`, `source_size_checker`, `development_paths`, `storage_settings`, `derived_data_cache`, `application_storage`, `shaders`, `plugin_applications`, `automation_connections` passed |
| Release deployment build | Passed using the independent ToolCache and alternate build/output roots; `BUILD_TESTING`, RenderDoc, Tracy, Triangle, DebugUI and source Content fallback disabled |
| Build helper and VS generation | `Build.ps1` reused locked independent sources; `GenerateSolution.ps1` produced the alternate solution with the existing target/configuration names |
| Source checks | Full paths/format/size checks passed; semantic naming passed for 39 changed C++ translation units, followed by the 3 units affected by the final case-equivalence fix |
| Dependency boundaries | Debug: 42 production/114 test targets; deployment: 38 production/0 test targets; both passed |
| DDC concurrency | Three processes, each with four threads, repeatedly published/read the same key without accepting partial or incorrect payloads |
| Deployment behavior | Copied exe/DLL/Content to a new installation, denied write access while retaining execution, launched from an unrelated working directory; cold, warm, corrupted and blocked-file cache cases passed; installation hashes unchanged |
| OpenSpec and diff | Strict change validation and whitespace checks passed |
| Accepted root-field layout | Label, expanding text field and trailing Browse button share each row; Debug Editor build, full format/path/size checks, naming for the two affected translation units, and `editor_capture_ui` passed; the resulting dialog screenshot was visually checked |

The initial harness failures were an empty-catalog test that did not disable the newly registered optional adapter, an isolated checker fixture missing its new Python dependency, and a test that did not restore Python's temporary-directory cache after testing environment isolation. They were corrected in their owning tests. The final case-equivalence regression rejects spurious Windows restart/revision changes without conflating empty saved defaults with explicitly selected roots.

Storage acceptance exercised real CLI and MCP discovery/schemas, pinned overrides, save/restart behavior, invalid and stale edits, persistence failure, current Game-root protection, provider disablement and graceful shutdown. It confirmed the default-system-directory substitute remained unused when selecting an off-system-drive locator and roots. Shared Config tests covered relocation, original preservation, legacy completion markers, isolation and failure state preservation; destination precedence was also checked in the copy-missing implementation.

Existing synthetic preference-dialog interaction and capture tests passed; the new storage controls were visually inspected in the captured dialog. GUI and automation save/error equivalence is established by their common service calls and domain/RPC tests. The new native folder-picker buttons were not separately driven in a new end-to-end storage workflow.

## Independent review

The `quality-audit` reviewer ran without inherited conversation context. Five confirmed findings were independently checked and repaired: system-default discovery leakage, nested migration accepted before restart, per-batch cache budget reset, missing protection for restored/switched Game content, and runtime isolation changing the default ToolCache. Targeted re-review closed all five. A final source review also accepted the Windows path case-equivalence fix. No open finding remains within the reviewed scope.

## Compatibility and retained evidence

- Historical logs, screenshots, performance data, review notes and old dependency trees remain in their original ignored locations. Only checksum-verified archives and the known configuration/state migration set are eligible for reuse; nothing bulk-copies historical evidence into tracked files.
- `GenerateModelFixtures.py` and `GenerateShadowFixtures.py` remain byte-for-byte compatible with external HyperionAssets recipe hashes. Their legacy standalone defaults remain available; supported build/test callers pass the new output directories explicitly. Game source caches remain owned by the selected external asset environment.
- Cache maintenance is bounded best effort, not a strict cross-process quota. Required shipped Content remains separate from disposable cache. Release validation covers deployment startup/rendering and cache recovery; the full CTest suite was run in Debug.

Raw logs, screenshots and review reproducers stay untracked under `out/build/debug/Storage*.log` and `out/tests/StorageDevelopment` / `out/tests/StorageReview` / `out/tests/Runs`. `out/tests/StorageDevelopment/FinalSnapshot.json` records the development snapshot before the accepted root-field layout adjustment and archive. A separate commit manifest freezes the archive delivery; these local artifacts are not required by a fresh checkout.
