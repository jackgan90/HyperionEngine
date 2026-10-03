# Verification — type-asset-preview-contracts

Date: 2026-10-03. Base: `7f6ce3b` on `main`, plus this uncommitted change. Implementation is limited to the asset preview contract refactor; the change remains active, without archive, commit or push.

## Compatibility baseline

Before production edits, Debug `editor_asset_documents`, `editor_asset_workspace` and `editor_asset_editors` passed (3/3, 28.43 s). The new attached preview acceptance script also passed against the original binaries while recording `Source/Tests/Automation/AssetPreviewSchema.json` (9.23 s).

That fixture captures discovery descriptions for `asset.preview.get/set` and reflected settings/state/edit types. Its SHA-256 remains `AAD202F65833348D12CC45D7027AFBCC3C022C23342E2F255A81705F22BFF571`. It was not regenerated after changing production code. Reflected integer fields, reflection definitions, operation IDs and versions remain unchanged.

## Builds and regressions

Both Ninja configurations built `hyperion_editor`, `hyperion_automation_cli`, `editor_asset_preview_contract_tests`, `editor_asset_workspace_tests`, `editor_asset_document_tests` and `automation_asset_tests` with MSVC toolset 14.50.35717 / C++20.

The existing configuration choices were preserved: BUILD_TESTING ON for both; RenderDoc ON in Debug and OFF in Release; Tracy OFF for both. Build logs are in `out/preview-contracts/DebugBuild.log`, `ReleaseBuild.log`, `DebugContractBuild.log` and `ReleaseContractBuild.log`.

Build through `tools/Build.ps1` so the shell initializes Visual Studio's compiler environment. For each preset, the following commands were used in the same shell:

```powershell
./tools/Build.ps1 -Preset debug -Target hyperion_editor
cmake --build out/build/debug --target editor_asset_preview_contract_tests hyperion_automation_cli editor_asset_workspace_tests editor_asset_document_tests automation_asset_tests --parallel 8
# Repeat with release / out/build/release.
```

The affected regression set passed in Debug (7/7, 87.68 s) and Release (7/7, 45.39 s):

```powershell
ctest --test-dir out/build/debug --output-on-failure -R '^(editor_asset_preview_contracts|editor_asset_workspace|editor_asset_documents|editor_asset_editors|automation_asset_preview|automation_capability_parity|automation_parity_regressions)$'
# Repeat with out/build/release.
```

- CPU contract tests use fixed expected wire values, captions, model paths and components for all three shapes and five channels, including invalid identities/values. Reversed and relabelled presentation spans still select the original semantic identity. Sky associations use distinct reference/texture sentinels and verify every named member. Format captions cover all supported formats and unknown rejection.
- Pixel tests exercise the actual extracted texture display implementation: every channel, RGBA alpha compositing, checker background and linear exposure/sRGB conversion. The original floating-point conversion/compositing algorithm is unchanged.
- Attached automation tests compare the frozen schema through MCP and JSONL, exercise every supported shape/channel, preserve readiness on equal/null patches, and reject invalid and unsupported settings without state changes. Invalid mixed patches also contain a valid checker/exposure change, verifying no partial mutation. Defaults, generation, dirty, undo and redo state are checked. Standalone preview access reports unavailable.
- Existing workspace, asset editor and automation parity regressions cover real preview preparation, GUI/history behavior, sky/material/texture previews, provider availability and controlled shutdown. Reordered/relabelled selections are tested at the presentation helper boundary; the new test does not synthesize separate real GUI clicks for each combo entry.

After the focused test's local assertion helper and stronger automation assertions were finalized, the Debug CPU test and attached test were rerun successfully. Release's seven-test run included those changes. The final formatted automation script passed again in Debug (7.46 s) and Release (4.45 s). Generated test logs/fixtures remain under `out`.

## Checks and manual review

- `python tools/CheckStyle.py`: path/include conventions and complete owned C++/HLSL formatting passed.
- `CheckStyle.check_naming` with the Debug compile database and Visual Studio environment: all seven changed C++ translation units passed clang-tidy/clang-query semantic naming and local declaration checks; included new headers were covered through these units.
- `python tools/CheckBoundaries.py`: passed. The new private test initially used `Support/TestSupport.h` across a module boundary; it now owns a small local assertion helper, and the extra test include path was removed.
- `git diff --check` and `openspec validate type-asset-preview-contracts --strict`: passed.
- Python syntax was parsed successfully; helper identifiers and layout follow the repository's Python conventions.

Manual review checked typed identity versus labels, the single authoritative shape/channel definitions, explicit sky member associations and format mapping, shared GUI/automation validation before mutation, and semantic equality for invalidation. No native asset keys, plugin dependencies, transport branches, renderer algorithms, async ownership or GPU retirement rules changed.

The 13 changed/new C++ files are at most 392 physical lines; changed/new functions fit the 100-line guideline (`DrawPreview` is 100 lines). The unchanged 605-line `AssetWorkspace.cpp` remains outside this focused mapping refactor; its lifecycle decomposition is a separate follow-up under the existing-code migration rule.

One initial hand-calculated pixel expectation was corrected from 128 to 127: the existing float sRGB conversion at 1 is slightly below 1 and half-alpha compositing rounds down. This was verified against the unchanged original conversion expression; no production math was changed to satisfy the test. A bare CMake build attempt without the Visual Studio environment also failed on standard/Windows headers; builds through the repository script passed.

Validation is the affected regression subset, not the entire repository test suite. No outstanding failure remains within this scope.

## Quality-audit follow-up — 2026-10-03

Independent review identified PREVIEW-01 (P2): the registered preview contract CTest lacked an executable dependency on `hyperion_check`. Main-agent verification confirmed the generated Ninja dependency graph omitted that executable, so direct clean builds could lack it and incremental check builds could run stale code. Added only the missing dependency in Source/Tests/CMakeLists.txt. Generated Debug/Release graphs prove the test links before CTest; both configurations passed the focused six-test regression set (57.54 s / 28.67 s), including the preview contract. The original independent reviewer confirmed the repair and closed PREVIEW-01. No preview runtime behavior changed. Evidence: `out/QualityAudit/MaintainabilityFollowups/AuditReport.md`, build graphs and validation logs. The full aggregate suite and a regenerated VS project were not run during this follow-up.
