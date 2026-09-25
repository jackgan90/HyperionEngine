## 1. Import conversion

- [x] 1.1 Add typed texture/sky conversion settings, authoritative validation and incremental provenance.
- [x] 1.2 Register standalone PNG/JPEG conversion and expose matching AssetTool options.

## 2. Shared workflow

- [x] 2.1 Add reflected import request/result, capability and task contracts with a bounded Main-owned workspace.
- [x] 2.2 Publish the workspace from assets services with scoped root participation, update and drain lifecycle.
- [x] 2.3 Adapt asset.import and add discovery, validation and shared task queries without transport changes.

## 3. Editor integration

- [x] 3.1 Add Platform file selection and the File > Import Asset panel with applicable grouped settings.
- [x] 3.2 Connect completion refresh, root transitions and persistent task results across panel visibility changes.

## 4. Validation and documentation

- [x] 4.1 Add CPU regression coverage for formats, settings, freshness, identity, validation and workspace lifecycle.
- [x] 4.2 Add real automation and GUI acceptance for import, parity, task visibility and provider disablement.
- [x] 4.3 Update user/developer documentation and capability inventory.
- [x] 4.4 Run affected builds, tests, style/boundary checks and strict OpenSpec validation; leave the change active and uncommitted.

## Validation evidence

- Debug and Release: built Editor, AssetTool, automation CLI and import workspace tests. Debug native folder/file picker test also built.
- CPU: import workspace passed in both configurations, covering PNG, HDR/EXR, settings, stable identity, zero-write repeats, explicit physical source roots, invalid requests, failed conversions and busy/drain/root-change lifecycle.
- Integration: JSONL and MCP import acceptance passed for glTF/GLB, PNG/JPEG, sky recipes/HDR, typed JSON and native paths. Real GUI clicks passed with shared task inspection and with automation disabled. Captured panel layout was visually inspected.
- Existing regressions passed: automation contracts/assets/transport, glTF import, native publication and CLI publication, plugin runtime/application absence paths and environment preprocessing, including their fixture setup tests.
- Native picker: cancellation and Unicode folder/file selection passed. A generated Debug object caused an initial COMDAT link failure; regenerating that object resolved it, and the picker was rebuilt and rerun successfully.
- Full source format/path and module boundary checks passed; semantic naming checked all 23 affected translation units and rechecked the final two modified units. Strict OpenSpec validation and git diff whitespace checks passed.
- Detailed local logs: `out/ImportRegression.log`, `out/ImportFinalAcceptance.log` (workspace/GUI passed; initial missing picker executable resolved by `out/ImportDialogBuild.log` and `out/ImportDialogTest.log`), `out/ImportCompatibilityFinal.log`, `out/ImportReleaseBuild.log`, `out/ImportReleaseTest.log`, `out/ImportStyleFinal.log`, `out/ImportNaming.log`, `out/ImportNamingFinal.log`, `out/ImportBoundariesFinal.log`, `out/ImportSpecFinal.log`.

At implementation completion, the change was left active and uncommitted as requested. It was subsequently synchronized and archived on 2026-09-25 under separate user authorization.
