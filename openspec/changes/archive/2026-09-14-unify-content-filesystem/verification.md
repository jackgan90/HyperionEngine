# Content migration acceptance

Archive note: the records below describe the uncommitted acceptance and audit snapshots. The user subsequently authorized OpenSpec archival and commits in both repositories on 2026-09-14.

Date: 2026-09-14. Engine checkout: `F:/HyperionEngine`; content checkout: `F:/HyperionAssets`.
The implementation and content migration are available for local acceptance. Neither repository has been staged or committed. Remote LFS publication and release revision pairing are outside this acceptance run.

The subsequent [independent quality audit](audit.md) records four confirmed defects, their fixes and final regression results. Its first full Release run also records a startup-readiness failure; the earlier acceptance results below are retained as historical evidence rather than rewritten.

## Delivered structure and behavior

- `/Engine` maps to engine `Content`: 24 HLSL/HLSLI files, the shared environment BRDF texture, and an engine catalog.
- `/Game` maps to HyperionAssets: published native models/scenes/skies and dependencies, the import identity index, sample text shaders, and source metadata/attribution.
- All 132 external hasset files match the Git LFS filter. Text shaders and metadata remain ordinary Git files. Original inputs are ignored under `.cache/Sources`.
- IO owns frozen mount configuration, package normalization, case/link checks, reads, enumeration/ReadTree, atomic writes and leases. Native caches, catalogs, references and scene persistence use canonical package paths. Shader source/include access uses the same filesystem; shader cache storage stays local.
- Viewer builds consume published content. `PrepareContent.py` explicitly restores the pinned source list/authored recipes and invokes AssetTool. Generator fingerprints normalize text line endings so Git checkout settings do not change generator identity.

Runtime setup and API details: [ContentFileSystem.md](../../../../docs/ContentFileSystem.md). Asset provenance: [Sources.json](../../../../../HyperionAssets/Metadata/Sources.json).

## Migration and content checks

The catalog validates a graph of 132 assets including its root and the Engine BRDF dependency. The migrated import library contains 229 identity entries with no machine-path keys. All eight publication roots report `Up to date` and `written_assets=0` on the final offline rebuild.

Only verified published roots/dependencies and the identity index were transferred from the old output. A blanket copy was rejected by automatic approval review; the accepted migration used the explicitly inspected asset list. The old source/shader/output directories were subsequently moved within the engine's ignored `out/migration` recovery area. There is no active `assets`, `shaders` or `out/content` directory.

Evidence:

- [Final offline rebuild and catalog validation](../../../../out/migration/RebuildFinal.log)
- [Exported import identity library](../../../../out/migration/NewLibrary.json)
- [Local LFS clean/smudge round trip and attribute checks](../../../../out/migration/LfsVerification.json)
- [Mount configuration precedence](../../../../out/migration/MountPrecedence.log)
- [Junction rejection](../../../../out/migration/JunctionRejection.log)

The LFS round trip reproduces the exact Sponza root bytes. This verifies local filtering and retrieval from the local object store; no remote push, server availability or fresh remote clone is claimed.

## Relocation, rendering and persistence

A separate directory with spaces and Chinese characters contains copies of 134 explicitly selected Game files and engine Content. Its mount directories are relative to its own config file. Viewer runs from that directory, with no original source/cache files in the copied Game tree and the old engine resource directories unavailable.

Each Sponza run uses 6,200 frames with 6,000 warmup frames. All 200 measured frames contain 79 scene draws, three point lights and zero failed items; GPU validation errors are zero.

| Check | Result |
| --- | --- |
| Relocated Sponza vs pre-migration capture | max difference 0, mean 0, changed channels 0 |
| Saved scene reloaded through `/Game` | complete native graph; no machine paths in exported scene; identical pixels |
| Cloudy, Dusk and Clear environments | ready native graphs and successful rendering; Dusk/Clear captures differ from Cloudy |
| SharedAssets scene | external `/Game/Shaders/SharedAsset.hlsl` renders successfully |
| Missing native LFS payload | explicit `git lfs pull` diagnostic |
| Unknown mount | explicit failure, no local-path fallback |
| Engine write | rejected by the read-only mount |
| Physical input and virtual output alias the same file | import rejected; source bytes unchanged |

[Relocation results](<../../../../out/migration/Relocated 内容/Results.json>), [sky image differences](../../../../out/migration/SkyImageDifferences.json), [acceptance script and assertions](../../../../out/migration/ContentAcceptance.py).

## Build and regression evidence

- Debug and Release builds complete with the new structure. Debug incremental-link COMDAT errors were resolved by regenerating build artifacts; no source workaround or weakened Debug/validation setting remains.
- Filename/include casing and formatting pass. Module boundaries pass for 478 files and 29 modules. The full semantic naming check passes for 312 translation units; final changed-unit checks also pass.
- Debug full regression passed 67/67 during implementation. After the final filesystem/shader changes, all 11 affected Debug tests pass, including native publication, shared material rendering, shader compilation and RenderDoc.
- Final Release IO, Shader, Triangle and full RenderDoc tests each pass three consecutive runs. Shader tests cover DXIL/SPIR-V/MSL, warm-cache reuse, physical relocation and include-content invalidation.
- Final Release broad regression passes 65/65 with no parallel build running; the known capacity issue described next is explicitly excluded from that run.

[Debug full run](../../../../out/migration/CTestDebug.log), [final Debug affected run](../../../../out/migration/CTestDebugFinalReadTree.log), [Release repeated checks](../../../../out/migration/CTestReadTree.log), [final Release broad run](../../../../out/migration/CTestReleaseFinalReadTree.log), [format](../../../../out/migration/StyleCheckFinal.log), [boundaries](../../../../out/migration/BoundariesFinal.log), [full naming](../../../../out/migration/Naming.log), [final naming](../../../../out/migration/NamingFinalChanged.log).

## Known independent regression finding

The warm-cache `deferred_rendering` capacity case can fail at frame 2 when 32 independent material resource sets toggle clustered lighting. The underlying error is `Material descriptor capacity exceeded (contiguous range unavailable): heap=1 requested=7 capacity=512`.

To isolate this from the mounted shader implementation, the test was rebuilt with the HEAD version of the original direct-file shader compiler and the original shader directory retained in the recovery area. That control passed cold and reproduced the same descriptor failure warm. This is a shader-loading control experiment, not a claim that the entire historical commit was rebuilt. Renderer descriptor allocation code is unchanged by this task. The issue is recorded separately rather than changing its assertions, heap limits or rendering behavior as part of the content migration.

[Cold control](../../../../out/migration/LegacyShaderCapacity.log), [warm control](../../../../out/migration/LegacyShaderCapacityWarm.log), [diagnostic error](../../../../out/migration/PreparationDiagnostic.log). Temporary diagnostic and control source edits were restored.

The migration did expose extra mounted shader file-access cost affecting early Triangle capture. Combining validated directory enumeration and reads through ReadTree removed the repeated per-file path work. The final tests retain the original 16-frame Triangle verification and RenderDoc capture frames 8/16; no frame or image threshold was relaxed.

## Local acceptance

From the engine repository:

```powershell
./out/build/release/bin/hyperion_viewer.exe --config experiments/Scene.json
./out/build/release/bin/hyperion_viewer.exe --scene /Game/Scenes/SharedAssets.hasset
./out/build/release/bin/hyperion_asset_tool.exe --mounts ContentMounts.json validate /Game/Catalog.hasset
```

The default configuration uses sibling checkout locations. A local override is optional; no temporary override remains. Recovery data and large test captures are ignored local files, and neither Git index was changed.
