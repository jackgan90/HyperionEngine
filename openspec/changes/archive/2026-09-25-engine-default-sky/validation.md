# Validation

Implementation and audit repairs completed on 2026-09-25. The records below describe validation before delivery. The user subsequently authorized archiving and committing both repositories; the change was archived on 2026-09-25 with all three requirements synchronized to the main specification.

## Content migration

- Moved Cloudy (`e0322173addca5129a20f3ca7aa47713`) and its radiance/specular native textures to Engine. Preserved their IDs and baked values; Engine BRDF remains shared.
- Scanned all Game native headers. Only Sponza required reference rewriting. Used the engine's Decode/ReadRecord/VisitRecord/EncodeAsset path, preserving provenance and regenerating dependency tables/revisions. Temporary migration build hook was removed from AssetTool source before final builds.
- Updated HyperionAssets Sources.json to reference `/Engine/Skies/Cloudy.hasset`; removed Game Cloudy output/source recipes and migrated source attribution into Engine metadata.
- `hyperion_asset_tool validate-library /Engine`: 14 native assets and graphs passed without Game.
- `hyperion_asset_tool --asset-root F:/HyperionAssets validate-library /Game`: 125 native assets and graphs passed.
- Tested offline source restoration and rebuilding in an isolated Engine copy, then exercised `PrepareContent.py --engine-sky --offline --cache out/default-sky/Sources --assets-root F:/UnusedDefaultSkyRoot` against shipped content. Same sky identity/revision; second import reported `written_assets=0`. No Game mount needed. Import provenance now records Engine publication.

## Code and build

- Release build of all targets passed; subsequent Editor incremental build passed after preview graph preflight was added.
- `CheckStyle.py`: filename/include casing and formatting passed.
- `CheckBoundaries.py`: passed.
- `openspec validate engine-default-sky --strict`: passed.
- `git diff --check` in both repositories: passed.

## Regression tests

13 selected CTest tests passed: automation_scene, editor_asset_workspace, editor_asset_documents, editor_asset_editors, scene_management, scene_runtime_instance, plugin_runtime, plugin_applications, sky_rendering, automation_parity_regressions, engine_default_sky, dependency_boundaries, code_style_paths. Log: `out/default-sky/Tests.log`.

After adding the missing-sky scenario, `engine_default_sky` passed again (`out/default-sky/DefaultSkyFinal.log`). Coverage includes discovery/description/invocation, stale revisions, shared history, active settings restoration, scene save/reload, Engine-only model/material previews, document isolation, missing-resource diagnostics and normal shutdown. C++ coverage additionally checks replacing an existing disabled environment, preserving intensity/yaw and busy rejection.

## Live rendering

- Inspected real Editor screenshots: `out/default-sky/Live/ModelSky.png` and `MaterialSky.png` show the built-in sky and lit native assets.
- Scene screenshot: `out/default-sky/Live/SceneSky.png`.
- Opened migrated Sponza, checked its active environment reference, waited for renderer readiness, captured `out/default-sky/Sponza/Sponza.png`, verified clean state and closed normally.
- Engine-only acceptance shutdown reported zero GPU validation errors.

## Audit repair validation

- SKY-01: test fixture preparation now combines the separate Engine and Game source manifests. Both the main agent and independent reviewer generated fresh fixture directories and verified Cloudy HDR against the manifest SHA-256. The fixture contract also checks missing Engine cache diagnostics.
- SKY-02: the Engine recipe supplies its canonical root ID through the shared importer, CLI and reflected automation option. Both agents independently rebuilt missing and corrupt sky outputs in isolated Engine roots; canonical identity and dependency graphs passed, and the next incremental run wrote zero assets. C++ coverage verifies reference resolution and rejects invalid IDs, mismatched existing IDs and duplicate library identities without replacing the original asset.
- Full Release build, style and module boundary checks passed. Nine relevant CTest tests passed across `AuditRepairTests.log` and `AuditRepairParity.log`: model_fixtures, sample_source_fixtures, environment_preprocessing, automation_capability_parity, engine_default_sky, native_asset_publication, shared_asset_publication, native_publication_cli and source_fixture_contracts. The first automation run encountered an old random-ID test output; the canonical-ID regression now uses a separate output path and passed on rerun.
- Evidence: `out/default-sky/AuditRepairBuild.log`, `AuditRepairRebuild.log`, `AuditRepairSnapshot.json` and `AuditRepairReview.md`. Independent targeted review confirmed both fixes and found no new confirmed defects in their direct scope. All 36 snapshotted files remained unchanged during review; only completion records were updated afterward.
- OpenSpec remains active, and neither repository has staged changes or commits from this task.

## Delivery scope

HyperionAssets: modified Sponza and two metadata files; deleted Cloudy and its two baked textures. Engine: three new native assets, source/license metadata, default reference, shared scene action, GUI/automation integration, preview integration, rebuilding helper, documentation and regression tests. No renderer algorithm, transport or plugin lifecycle changes. No archive and no commit.
