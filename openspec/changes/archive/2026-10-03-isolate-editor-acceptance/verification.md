# Verification

Source baseline: `f6c202af2af66b3e3435a08258b2ba570f04d01f`, with this change applied locally. Validation date: 2026-10-03. This is a structural ownership change; the original finding was not a reproduced user-facing defect. The change remains active and uncommitted.

## Ownership and compatibility review

- Migrated 121 scenario methods across 30 source files from `FEditorPlugin` to `FEditorAcceptanceHarness`, along with the former driver's scenario state and 21 production-held snapshots. Reversing the ownership qualifications yields identical C++ tokens for those scenario bodies. Existing step sequences and assertions were preserved; the separate driver panel hook was checked with the lifecycle hooks.
- `FEditorAcceptanceDriver` stores only an incomplete implementation pointer. A test-enabled ordinary run does not create the harness. Test-disabled source selection links the unavailable driver and shared report formatter without scenario headers or implementations.
- Production supplies typed widget/surface observations and explicit clock, VSync, frame-limit, asset-window/layout, outline-selection and capture hooks. Fixture IDs, case filtering, report result projection, assertions and remembered UI bounds belong to the harness. The actual profiling popup anchor remains production GUI state.
- Main consumes the outline selection span, resolves VSync from the copied frame settings and acceptance policy, and freezes render inputs before dispatch. Capture completion runs after the existing render wait. No borrowed acceptance state crosses execution domains, no new asynchronous work or provider lifetime is introduced, and the existing document/asset/render services remain authoritative for GUI and automation.
- CLI flags, JSON keys/types, serialized data, plugin IDs, operation registration and domain behavior are unchanged. The source changes add no per-frame logging or public automation capabilities.
- Fixed control identity is typed; legacy observation keys have one explicit mapping inside acceptance. Startup policy is projected in one place. Existing observation reset/retention points and GUI call order are preserved. Outliner filtering reuses one private text-filter implementation.

## Size and scope review

All affected production C++ files are at most 500 physical lines. `EditorApplication.h` is 486 lines, `EditorPanels.cpp` 458, `EditorOutliner.cpp` 136 and `EditorInspector.cpp` 226. Startup options and interaction value types have their own private headers.

New production/acceptance glue functions are within the 100-line guideline. Four existing scenario step functions remain longer: `ExerciseAssetPropertyInput`, `ExerciseAssetTextureInput`, `ExerciseDocumentInput` and `ExerciseImportInput`. Their implementation was moved and qualified without changing logic; this change deliberately preserves their existing coupled multi-frame steps and assertions. Further scenario organization and unrelated oversized Editor subsystems are outside this scope.

## Builds and structural checks

- Debug: `tools/Build.ps1 -Preset debug`, BUILD_TESTING=ON, RenderDoc=ON, Tracy=OFF — passed.
- Release: `tools/Build.ps1 -Preset release`, BUILD_TESTING=ON, RenderDoc=OFF, Tracy=OFF — passed.
- Production: separate Ninja Release configuration at `out/build/acceptance-production`, BUILD_TESTING=OFF, RenderDoc=OFF, Tracy=OFF; built and linked `hyperion_editor` and `hyperion_automation_cli` — passed.
- `EditorAcceptanceBoundary.py` passed against both enabled and disabled compilation databases, including production transitive private-header isolation and actual source selection.
- Library symbol comparison found 122 Exercise/Prepare/Check harness symbols in the enabled library and none in the disabled library. The disabled library also contains no `FEditorAcceptanceState` type symbols.
- `python tools/CheckStyle.py` — passed paths and full formatting.
- Repository clang-tidy/clang-query naming checks on all changed translation units — 55 enabled units and 1 disabled implementation passed.
- `python tools/CheckBoundaries.py`, `git diff --check` and `openspec validate isolate-editor-acceptance --strict` — passed.

## Runtime evidence

Before editing, the existing Release Editor/automation-parity/plugin subset passed 26/26. A three-frame ordinary run produced the baseline 40-key JSON report and PNG.

After editing, the Release Editor/GUI/automation-parity/plugin/style/boundary subset passed 36/36 (178.49 seconds). The Debug expanded subset passed 43/43 (582.38 seconds), including automation capability parity, RenderDoc HUD, plugin runtime/lifecycle recovery, capture controls and capture failure recovery. Tests use isolated LOCALAPPDATA directories; GPU test suites run sequentially.

The exact CTest selections were:

```powershell
$env:LOCALAPPDATA = 'F:/HyperionEngine/out/AcceptanceIsolation/ReleaseLocalAppData'
ctest --test-dir out/build/release --output-on-failure -R '^(editor_|gui_|automation_parity_regressions$|automation_connections$|automation_scene$|plugin_applications$|plugin_lifecycle$|lifecycle$|dependency_boundaries$|code_style_paths$)'
$env:LOCALAPPDATA = 'F:/HyperionEngine/out/AcceptanceIsolation/DebugLocalAppData'
ctest --test-dir out/build/debug --output-on-failure -R '^(editor_|gui_|automation_parity_regressions$|automation_connections$|automation_capability_parity$|automation_renderdoc_hud$|plugin_applications$|plugin_runtime$|lifecycle_recovery$|capture_controls$|capture_failure_recovery$|dependency_boundaries$|code_style_paths$)'
```

The Release selector includes three historical names (`automation_scene`, `plugin_lifecycle`, `lifecycle`) that match no tests in this configuration; the reported 36 count is CTest's actual executed count. Plugin runtime and lifecycle recovery were explicitly covered by their current names in the Debug run.

Ordinary execution and compatibility checks used fresh isolated layout/preferences directories for each run:

- Enabled Release and disabled-production Editor, three hidden frames with `--report` and `--capture`: both returned success, produced all 40 baseline keys with identical values, and generated PNGs byte-identical to the baseline (1600×960, 167203 bytes). Fresh directories matter because completed ordinary runs save layout state.
- Disabled-production benchmark on the isolated native content fixture, two warm-up frames and three samples: three CSV samples, five ready frames, one scene draw, a valid PNG and compatible 40-key report; zero GPU validation errors.
- Disabled-production boolean, path and capture acceptance requests: each returned exit code 1 with `Editor acceptance is unavailable: configure BUILD_TESTING=ON`.

JSON number compatibility treats integer and fractional representations as the same JSON type; for example scene framing changes the numeric movement speed. No application source change was needed for this validation-script distinction. Early three-frame captures can also differ while asynchronous placement previews become ready, so pixel identity is reported only for the recorded final fresh-directory runs.

Raw build logs, CTest logs, symbol comparison, token comparison helpers, size inventory and runtime outputs are retained under `out/AcceptanceIsolation` as local, ignored evidence. They are not repository requirements or shipped configuration.

## Independent audit and repair follow-up

Three independent reviewers audited the frozen 76-path working-tree snapshot against the same baseline: production/runtime boundaries, scenario migration, and build/report compatibility. The main agent independently confirmed both findings and made five code/test-file repairs; no scenario body, assertion, step or snapshot definition was changed during this audit. The original reviewers then re-reviewed the frozen repair snapshot and closed both findings.

| Finding | Confirmed issue | Repair and disposition |
| --- | --- | --- |
| BND-001 / P2 | The new boundary CTest unconditionally opened `compile_commands.json`, causing supported Visual Studio configurations to fail. The original failure was reproduced in Debug, Release and RelWithDebInfo. | CMake generates a per-configuration inventory from the actual Editor target's `SOURCES`. The check always validates that inventory and additionally validates the compilation database when present. Closed after independent generator and negative-case verification. |
| RUNTIME-01 / P2 | `ExecuteEditorGraph` accessed the Main-only acceptance policy from Render, contrary to this change's explicit isolation contract. The read-only policy and synchronous wait did not demonstrate a data race. | Main computes the existing VSync conjunction from copied frame settings and passes its bool value through Render/RHI. Both RenderDoc paths preserve the original conditions and waits. Closed after independent source and validation-evidence review. |

Post-repair validation:

- Rebuilt and linked Ninja Debug Editor (RenderDoc ON), Release Editor (RenderDoc OFF), and the existing BUILD_TESTING=OFF production Editor/Automation targets.
- The focused CTest selection below passed Debug 5/5 (215.25 seconds) and Release 5/5 (46.61 seconds), with isolated LOCALAPPDATA and sequential GPU execution:

  ```powershell
  ctest --test-dir out/build/debug --output-on-failure -R '^(editor_acceptance_boundary|editor_acceptance|editor_capture_ui|editor_render_controls|editor_outlines)$'
  ctest --test-dir out/build/release --output-on-failure -R '^(editor_acceptance_boundary|editor_acceptance|editor_capture_ui|editor_render_controls|editor_outlines)$'
  ```

- A real Visual Studio 2022 audit configuration, without a compilation database, passed `editor_acceptance_boundary` for Debug, Release and RelWithDebInfo. The build reviewer independently reran all three and matched their acceptance source inventories to the generated `.vcxproj` compilation items. A complete Visual Studio solution build/test run was not performed in this follow-up.
- Ninja Debug/Release/production inventories and compilation databases all passed. The build reviewer also verified six incorrect ON/OFF inventories and one correct inventory paired with an incorrect compilation database are rejected using isolated copies.
- Re-ran all six ordinary/disabled-runtime cases: ON/OFF 40-key reports and PNGs exactly match the original baseline; OFF benchmark emits three samples with five ready frames, one draw and zero GPU validation errors; three unavailable acceptance requests retain exit code 1 and the existing diagnostic.
- Full formatting/path checks, module-boundary checks, naming checks for the two modified translation units, `git diff --check`, and strict validation of this OpenSpec change passed. Prior broader implementation suites remain recorded above; they were not repeated without a new affected concern.

All 121 migrated scenario methods, existing state defaults and 21 transferred snapshots were independently reviewed; no additional confirmed migration defect or unresolved migration question was found. Reviewer GPU execution was not independent: reviewers inspected the main agent's raw runtime results, and the runtime reviewer independently compared the resulting report dictionaries and PNG bytes with the baseline.

Audit reports, initial and repair hash manifests, raw build/test output and runtime artifacts are in `out/AcceptanceIsolationAudit`. The repair snapshot SHA256 is `e73f6a7e3c013e8af28558562a7dc7b54841b61489329aa2c36cbf2574d1f900`; after review only this verification record was updated. HEAD and the empty index are unchanged; no archive, commit or push was performed.
