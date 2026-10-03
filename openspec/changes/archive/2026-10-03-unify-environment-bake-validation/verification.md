# Verification — unify-environment-bake-validation

2026-10-03, base `7f6ce3b` on `main`, alongside the existing uncommitted `type-asset-preview-contracts` change. This change is complete and remains active; no archive, commit or push.

## Scope and compatibility

Environment owns FEnvironmentBakeLimits and the bake/prefilter numeric predicates. AssetImport and actual baking use those predicates; metadata and the import range diagnostic derive from the same limits/defaults. Image admission, source format/type admission and standalone captured-cube policy remain separate. Pixel/filter algorithms, source cache keys, reflected fields/version, legal values, defaults and exact previous diagnostics are preserved.

Production scope is SkyAsset.h, EnvironmentBake.cpp and ImportSettings.cpp. EnvironmentTests.cpp adds fixed boundary/cross-field/overflow, original metadata/diagnostic, incompatible import and actual small-bake/invalid-image cases. docs/SkyLighting.md documents ownership. No CMake dependency or lifecycle change was needed.

## Executed validation

Original Debug preprocessing and import-workspace baseline passed 3/3 including source fixtures (5.29 s).

Debug and Release built `environment_tests`, `import_workspace_tests`, `hyperion_asset_tool`, `hyperion_editor` and `hyperion_automation_cli` using MSVC toolset 14.50.35717 and the repository's Visual Studio environment setup. BUILD_TESTING remained ON; RenderDoc stayed ON for Debug and OFF for Release; Tracy remained OFF.

```powershell
./tools/Build.ps1 -Preset debug -Target environment_tests
cmake --build out/build/debug --target import_workspace_tests hyperion_asset_tool hyperion_editor hyperion_automation_cli --parallel 8
ctest --test-dir out/build/debug --output-on-failure -R '^(environment_preprocessing|asset_import_workspace|editor_asset_import|import_draft_automation)$'
# Repeat in the same initialized shell with release / out/build/release.
```

Both configurations passed 5/5 including the source fixture: Debug 74.53 s, Release 20.74 s. Coverage includes real HDR/EXR import, incremental/native publication, workspace validation, GUI and attached automation import, and draft discovery/invocation. Logs are in `out/maintainability/BakeValidation/{Debug,Release}{Build,Tests}.log`.

Complete owned formatting/path checks passed. Semantic naming/local declaration checks passed on all three changed C++ translation units and their included header. Dependency boundaries passed (39 modules); `git diff --check` and OpenSpec strict validation passed. Changed production files remain below 500 lines and changed/new functions below 100 lines. Manual review checked authoritative limits, validation ordering and ownership, unchanged exception categories/messages, no new renderer dependency and no semantic/cache behavior change.

Maximum bake dimensions are validated without allocating maximum-size products; numeric rendering behavior is covered by existing deterministic small-image tests. This is the affected regression set, not a full repository suite. No unresolved validation failure remains.
