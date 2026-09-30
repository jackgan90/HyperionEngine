# Import contracts and Editor host validation

## Implementation scope

- Tasks 5.1–5.3: `ImportChoices.h` is the actual panel's capability-backed type/filter/settings adapter. Selection persists `Request.Type`; combo indices are rebuilt presentation positions. Model, Texture, Sky and Auto retain the existing source formats; explicit mismatches remain domain validation failures. Six capability permutations exercise explicit/auto resolution, uppercase extensions, filters and setting eligibility.
- Texture draft inspection adds reflected `dimension` (`ETextureDimension`) and `pixelBytes` (`uint64`, wire decimal string), after existing fields. Existing width/height, format, encoding and human-readable Details remain. GUI directly displays typed values. Tests cover 2D and cube metadata, all mip/face bytes, schema enum descriptions, wide-integer wire round trip, and real JSONL/MCP discovery and invocation.
- `ImportDraftPreviewPageLimit` (32) explicitly controls every Editor draft query and page navigation; the unchanged API maximum is `ImportDraftMaxPageLimit` (64). A valid 69-node model proves contiguous page traversal and return to the previous page.
- Automation response framing shares the actual response invariant; job defaults, channel queue/write budgets, peer/session admission, correlation/handshake sizes, dispatch, request/connection capacity, shutdown drain and probe timeouts retain all existing values. Unrelated values of 32 remain separately named. Angle helpers retain the former float factor and replace both repeated vector conversions.
- Additional delegated 6.3 scope: `EditorHost.cpp` separates startup options, service registration, plugin descriptor, selection and validation. `HasEditorAcceptanceRequest` includes every current exercise flag/path and is reused by interactive/root-restoration and missing-Editor checks. The original GUI persistence exceptions for log/content/import remain explicit; framing is isolated. Plugin composition and Start/Run/Stop ownership remain unchanged. `PluginAcceptance.py` now checks framing/import failure for kernel-only and disabled graphics/gui/editor.

## Completed verification

Commands use the repository `tools/Build.ps1`; CTest is the Visual Studio bundled executable. Existing Release build tree was coordinated exclusively with other implementers. Generated-file writes needed sandbox escalation, which succeeded; no source/config dependency installations occurred.

| Command / check | Result |
| --- | --- |
| `tools/Build.ps1 -Preset release -Target editor_import_choices_tests` | PASS |
| `tools/Build.ps1 -Preset release -Target import_workspace_tests` | PASS |
| `tools/Build.ps1 -Preset release -Target automation_connection_tests` | PASS |
| `tools/Build.ps1 -Preset release -Target automation_tests` | PASS |
| `tools/Build.ps1 -Preset release -Target automation_scene_tests` | PASS |
| `tools/Build.ps1 -Preset release -Target hyperion_automation_cli` | PASS |
| `tools/Build.ps1 -Preset release -Target hyperion_editor` | PASS |
| Release CTest `editor_import_choices`, `asset_import_workspace` | 2/2 PASS; 2.61 s |
| Release CTest `automation_connections` | PASS; 1.42 s |
| Release CTest `automation_contracts`, `automation_scene` | 2/2 PASS; 2.46 s |
| Release CTest `import_draft_automation` | PASS; 4.70 s; new schema/metadata checks execute in JSONL and MCP |
| Release CTest `editor_asset_import`, `plugin_applications` | 2/2 PASS; 18.46 s; actual GUI typed metadata assertion and new framing/import absence cases |
| `tools/Build.ps1 -Preset release -Target automation_asset_tests` | PASS; includes shared workflow regression source |
| Release CTest `automation_assets`, `automation_transport` | 2/2 PASS; 14.37 s; workflow success/failure/close/drain and shipped transport paths |
| Existing Release cache `cmake --preset release -DBUILD_TESTING=OFF`, then `tools/Build.ps1 -Preset release -Target hyperion_editor` | PASS; actual production compile and link |
| Test-disabled generated compilation graph | PASS; every Editor TU uses `HYP_BUILD_TESTING=0`; real acceptance driver/fixtures are absent; only `EditorAcceptanceUnavailable.cpp` supplies the controlled adapter and ordinary production capture decision |
| Test-disabled real Editor startup | 9/9 PASS: ordinary hidden frames, kernel-only, disabled graphics, disabled GUI, disabled Editor output rejection, and unavailable diagnostics for exercise/framing/selection-shortcuts/import |
| Final coordinated Release CTest group after restored-ON rebuild | 8/8 PASS; 21.98 s; scene 1.57 s, assets 1.20 s, choices 0.44 s, contracts 0.02 s, connections 0.09 s, transport 13.69 s, import workspace 1.55 s, draft automation 3.20 s |
| Final ownership-repair BUILD_TESTING=OFF build checkpoint | PASS compile/link; generated graph again excludes real acceptance implementations; real hidden 8-frame and unavailable-framing smokes both PASS |
| Restored BUILD_TESTING=ON final Release artifacts | PASS; Editor/CLI and all related CPU test targets rebuilt; final Editor increment includes the GUI button-count constant change |
| Restored test-enabled Editor ordinary startup | PASS; hidden 8-frame smoke exits successfully with zero graphics validation errors |
| Changed-file clang-format byte comparison | PASS; 25 import/angle/host/contract/test C++ files |
| `python tools/CheckStyle.py --paths-only` | PASS at 966-file snapshot |
| `python tools/CheckBoundaries.py` | PASS at 929-source/38-module snapshot |
| Scoped `git diff --check` | PASS |

The synthetic pagination fixture initially lacked renderable geometry and used the wrong aggregate order for `FAssetRef`; both test-fixture problems were repaired and the workspace test rerun successfully. Initial Editor builds encountered concurrent owner-refactor compile errors outside this agent's scope; the owner repaired them and the final Editor build and GUI tests passed.

Test-disabled evidence is `out/acceptance/ConsolidationNoTests/Evidence.json` plus per-case logs. It records production TU count, excluded real acceptance source list, adapter path, actual binary SHA-256 and process exit outcomes. After the final acceptance-field ownership repair, `FinalOffCheckpoint.json` and `FinalOffHidden.log` / `FinalOffUnavailable.log` preserve another actual build/binary/header-hash checkpoint and two process smokes. The existing cache was restored with `cmake --preset release -DBUILD_TESTING=ON`; final test-enabled Editor, CLI and related CPU test artifacts have been rebuilt. `FinalOnSmoke.log` records successful ordinary hidden startup after restoration. The final eight-test log is `out/build/release/Testing/Temporary/LastTest.log`.

## Coordination boundary

- Final combined changed-scope style/boundary/OpenSpec checks and broader Debug integration belong to the other implementation owner. This file records only results actually executed by this agent.
- Any later acceptance-fixture-only changes are compiled by that owner; they do not require repeating the production OFF/ON cycle documented here.

No archive, commit or push was performed.
