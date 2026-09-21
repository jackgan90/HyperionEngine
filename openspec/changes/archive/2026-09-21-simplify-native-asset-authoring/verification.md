# Verification

Validation performed on 2026-09-21 against the uncommitted engine and content worktrees. No commit, push or staging was performed. Raw logs, original native backups and isolated migration candidates are retained under `out/asset-migration` (ignored build output).

## Delivered behavior

- Native metadata discovery rebuilds ID resolution without persisted catalog/import-library files. Duplicate IDs and malformed metadata are diagnosed. Bulk data is skipped during discovery and fully validated during loading.
- Native saves and imports preserve current asset identity, use portable package/relative references and clear authoring revision constraints. Explicit scene reload refreshes completed native caches; previously held snapshots remain valid.
- Imports discover current product mappings, publish visible stable filenames, reuse shared textures with matching native data/interpretation, preserve independently editable material identities and skip unchanged valid imports. Synchronous publication failures restore previous bytes and remove newly written files.
- The Editor browser exposes every project hasset without an internal-assets toggle. Dedicated non-scene editors, the Editor import dialog and background file watching remain outside this change.

## Native migration

| Root | Before | After |
| --- | --- | --- |
| HyperionAssets `/Game` | 133 hassets, including 2 management files | 128 current assets: 8 models, 34 materials, 79 textures, 3 skies, 4 scenes |
| Engine `Content` `/Engine` | 12 hassets, including Catalog | 11 current assets: 5 models, 1 material, 5 textures |

The five removed Game files are two management files, one unreachable historical Sponza generation and two accidental hidden/public aliases. The hidden Showcase model and Cloudy sky now resolve to their existing public identities. All other current reachable identities and the eight public entry paths are retained. Absent historical import mappings were not restored. Shader text, licenses and optional source recipes remain; source recipes were not used to overwrite authored scenes.

The isolated candidate was generated through `migrate-library` using the current engine's native readers. An independent metadata/bulk comparison confirmed every migrated object's authored values and binary blocks were preserved after accounting for existing model/scene schema migrations. The final Game tree has 128 unique IDs, no absolute machine paths, no pinned dependencies and no dangling optional import output IDs. The Engine objects were compared with their native backup as well. Both final trees match their verified candidates.

Before publication, all 143 originally tracked Game files were SHA-256 compared with the untouched backup. Publication copied 130 Game files (128 native files plus README and source manifest), removed 125 superseded tracked native paths, and removed the empty `.assets` directory. The Engine Catalog was also removed. Backups are in `out/asset-migration/baseline` and `out/asset-migration/baseline-engine`; no backup/history data was added to HyperionAssets.

Final actual-root checks passed:

```powershell
out/build/debug/bin/hyperion_asset_tool.exe --mounts ContentMounts.json validate-library /Game
out/build/debug/bin/hyperion_asset_tool.exe --mounts ContentMounts.json validate-library /Engine
```

Results: 128 and 11 native assets respectively, with every dependency graph load successful. See `validate-final-game.log`, `validate-final-engine.log`, `metadata-validation.json`, `final-inventory.json` and `published.json` under `out/asset-migration`.

## Rendering and reimport

The four shipped scenes were captured with the original content/engine and with the migrated isolated mounts. Each ran 900 hidden frames with model readiness checks on NVIDIA GeForce RTX 5080 / D3D12. All reported zero graphics validation errors. Every before/after PNG matched byte for byte:

| Scene | PNG SHA-256 |
| --- | --- |
| Showcase | `d46046c9e1ff4f4f6a7fc18d1c7b6a8282a4c1c760a7b75dc85232a36bb9f731` |
| Shadows | `cdbebd408c665de6982e239126e5b6011b9602ea2e4dcb0db687fa139470ec31` |
| SharedAssets | `dcfdefc19bbf11ee16d14504c841e5d0a7fd7e81ea1f4f6f56ed1f6eb07df49b` |
| Sponza | `e2cce0c1b24cd76726cfdde94d6282bf1804bd768a6a6db6958be2456a6af16a` |

An actual Showcase source reimport was also exercised against the staged library. The initial reimport aligned optional import metadata while retaining identity and authored values; the following unchanged reimport reported `written_assets=0`. Unit coverage additionally clones libraries to different physical directories and verifies unchanged reimport with both default and explicit logical source namespaces.

## Build and regression checks

- Full Debug and Release builds passed using `tools/Build.ps1` (VS 18 Build Tools / MSVC 14.50 / CMake 4.1 / Ninja).
- Release: all 12 selected tests, including their required fixture, passed. Selection covers native management/discovery/publication, shared publication/rendering, reflected archives, material contracts, scene runtime reload and Editor content startup/transition.
- Full owned-source formatting/path checks passed for 721 files. Semantic naming passed for all 471 C++ translation units; the two subsequently modified units were checked again after the final changes.
- Targeted regressions cover range-only discovery without bulk reads, corrupt payload rejection, duplicate IDs, relocation and ID resolution, cross-volume portability rejection, same-target identity, explicit independent roots, texture sharing, two-parent reload, unchanged reimport after cloning, failure rollback and held CPU/GPU snapshots.
- Final actual-root Debug suite: all 15 tests passed in 146.59 seconds, including full Editor integration (scene open, native editing/save/reload, viewport controls and error recovery), IO/mount contracts and dependency boundaries. Results are recorded in `debug-final-tests.log`.
- `git diff --check` and strict OpenSpec validation passed. Neither repository has staged changes.

Publication provides synchronous rollback at a quiescent authoring boundary. It does not claim multi-file atomic visibility for concurrent external readers or recovery after power loss. Importing source recipes can replace native authoring edits, as documented in the asset repository README.

The final Debug selection was:

```powershell
ctest --test-dir out/build/debug --output-on-failure -R '^(native_asset_registry|native_asset_management|native_asset_publication|shared_asset_publication|reflected_archives|editor_content|editor_content_transition|editor_content_startup|shared_asset_rendering|scene_runtime_instance|material_asset_contracts|async_io|dependency_boundaries|editor_acceptance)$'
```

The Release selection used the same expression without `async_io`, `dependency_boundaries` and `editor_acceptance`; CTest added the required model fixture in both configurations.

## Follow-up: descriptive project filenames

At the user's subsequent request, 120 project hassets were renamed: 34 materials, 7 models and 79 textures. Sponza materials were named from inspected texture content, and texture filenames use their native material binding roles. Existing meaningful names were retained without UUID suffixes; the unnamed Interleaved material uses a numbered name. All 128 IDs and shared relationships are unchanged. Reference paths and integrity digests were updated in 49 referring files, preserving every other field and bulk byte. Existing public entry paths, source recipes, local cache and Git history remain intact.

The actual directory again passed complete validation of all 128 dependency graphs. The four scene screenshots, with matched 1280x720 settings, remain byte-identical to the migration baseline with zero GPU validation errors. Showcase unchanged reimport in the isolated renamed library again reported zero writes. The engine's default generated-name policy was not changed by this data-only follow-up. The complete rename mapping, immediate backup and verification evidence are under `out/asset-renaming` (`RenamedAssets.csv`, `before`, `README.md`). Both repositories remain unstaged and uncommitted.

## Explicit quality audit

An independent reviewer without conversation inheritance reviewed the complete related uncommitted engine and asset changes against Engine `ab1c55315b73415e6f6d890e7aa9f0b6d9fa46b2` and Assets `41b0349d89d03eb044ed08cd1739e4537dbcb755`. The parent independently confirmed and minimally repaired three findings: conflicting products silently overwriting one native ID (NA-01/P1), stale texture fingerprints reusing changed staged pixels (NA-02/P1), and import/tool entry points following old paths after native renames (NA-03/P2).

Publication now rejects conflicting contents before writing, verifies actual staged reuse candidates and counts reused products in the ID conflict check. Import and single-root native tools build the appropriate discovery index. Incremental external-native source checks use full container fingerprints from the ID-resolved dependency graph, so repeated renames and reuse of old paths do not force writes, while actual native changes still invalidate freshness. This does not introduce automatic splitting of shared identities or a filesystem watcher.

The initial targeted re-review caught one remaining rename-after-external-import case. It was independently reproduced before correction, covered by the permanent CLI regression, and closed by the original reviewer in the final re-review. All three findings are closed; the final code snapshot has 208 matching file hashes. No additional confirmed defect remained within the reviewed scope.

Audit validation: full Debug and Release builds passed; the initial repaired selection passed Debug 15/15 and Release 13/13. After the last provenance correction, all four directly affected import tests (including the required fixture) passed again in both configurations. Full owned formatting/path checks, naming checks for changed units, dependency boundaries, and strict OpenSpec validation passed. All current 128 Game and 11 Engine assets and their graphs validated with zero writes. The reviewer independently verified preservation of authored values, bulk data and reference identity across migration and semantic renaming; actual native asset bytes were unchanged during the audit. Both repositories remain unstaged and uncommitted.

The audit report, initial and repaired hash manifests, independent reports, reproductions and raw validation logs are under `out/asset-audit`; see `Audit.md` and `reviewer/ReReviewFinal.md`. The reviewer independently exercised CPU/CLI checks; GPU checks in the repaired selection were run by the parent. Full-scene screenshot comparisons were not repeated because the audit did not change asset bytes or rendering code.

## Archive and commit authorization

After completion of the independent audit, the user explicitly requested OpenSpec archival followed by commits in both repositories. The earlier uncommitted-state statements record the implementation and audit acceptance boundary; this subsequent authorization permits the verified changes to be committed. Archive and commit evidence is retained under `out/asset-commit`.
