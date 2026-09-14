# Independent quality audit

Archive note: the records below describe the uncommitted acceptance and audit snapshots. The user subsequently authorized OpenSpec archival and commits in both repositories on 2026-09-14.

Date: 2026-09-14. Scope: the uncommitted content/filesystem migration in both repositories, including tracked diffs and new files. Engine baseline: `5a94faf7882745d53d96ce663bf1faaec1a0266c`; HyperionAssets baseline: `41d6b676ffe62379c367f7ba28cbd97313e9136d`. Existing external LICENSE is unchanged. Neither index nor commit was changed.

An independent reviewer received the requirements and repository/evidence locations without conversation history. Source files were frozen during each review. The implementing agent independently checked each finding before fixing it; the original reviewer then reviewed the fixes, including a second pass on the cache path alias case.

## Findings and disposition

All four findings were confirmed changes introduced within this task's scope. All are P2 and closed after targeted re-review.

| ID | Confirmed defect | Fix and regression coverage |
| --- | --- | --- |
| R1 | Shader include files such as `.inc` were accepted by DXC but omitted from the cache hash, returning stale bytecode after edits. | ReadTree includes all source-root files; cache identity advances to v9. Tests change Engine `.inc` and Game `.HLSLI` contents and check both identity and bytecode, retaining relocation and DXIL/SPIR-V/MSL checks. |
| R2 | Comparing a physical cache directory with a virtual shader root allowed generated files inside the source tree, including a read-only Engine mount. | Resolve the cache with write permission and case/link validation before any local operation, then reject shader-root containment. Tests cover Engine/Game with exact and incorrect `Shaders/shaders` spelling and verify no directory was created. |
| R3 | Mapping a mount to a drive root produced an erroneous doubled separator in containment checks; physical aliases bypassed read-only and overlap checks. | Handle existing root separators and join relative tails correctly. Tests cover drive-root normalization, physical aliases, write/lease rejection and overlapping mounts. |
| R4 | External native assets could change between graph validation and fingerprint reads, causing successful publication with an immediately invalid fixed revision. Repeated references could also overwrite an earlier fingerprint. | Validate the fingerprinted bytes against the graph's ID/type/revision and reject conflicting prior fingerprints. Deterministic storage injection replaces the dependency on its second or third read; both cases preserve the previous published root. Stable external references still publish and validate. |

Relevant code: [mounted filesystem](../../../../Source/Runtime/IO/Private/MountedFileSystem.cpp), [shader compiler](../../../../Source/Runtime/Shaders/Private/Adapters/ShaderCompiler.cpp), [publication graph](../../../../Source/Runtime/AssetImport/Private/AssetPublicationGraph.cpp). Regression tests: [IO](../../../../Source/Tests/IO/MountedFileSystemTests.cpp), [shader](../../../../Source/Tests/Shaders/MountedShaderTests.cpp), [publication](../../../../Source/Tests/Assets/PublicationTests.cpp).

## Verification

- Independent content checks validated the 132-asset catalog graph, 74 cached upstream download hashes, both generator fingerprints and the unchanged contents of all 24 engine shaders. External LICENSE and both Git indices were unchanged.
- Release and Debug builds of the affected programs succeeded. Formatting, filename/include casing, module boundaries and semantic naming of all six changed C++ translation units passed.
- The initial Release audit build ran all 66 tests: 65 passed. `cpu_frame_viewer` failed because its scene was not ready within the existing 160-frame warmup. This run followed shader cache version invalidation; it does not establish the cause of the readiness failure.
- On the final code, all nine selected Release tests (including two source fixtures) passed twice consecutively. This includes IO, shaders, native/shared publication, Triangle, RenderDoc and the unchanged `cpu_frame_viewer` test. No frame, image or readiness threshold was relaxed.
- Final Debug related regression passed all eight tests (including two source fixtures): IO, shaders, native publication, Triangle, RenderDoc and `cpu_frame_viewer`.
- Standalone post-fix reproductions confirm changed includes produce different keys/bytes matching a fresh compiler, read-only cache construction is rejected, replacing an external dependency is rejected before publication, and a physical drive-root alias can no longer write through the read-only mount.

Logs: [initial Release run](../../../../out/audit-content/BuildRelease.log), [final Release build](../../../../out/audit-content/BuildReleaseFinal.log), [final Release repeated tests](../../../../out/audit-content/CTestReleaseFinal.log), [Debug build](../../../../out/audit-content/BuildDebug.log), [Debug tests](../../../../out/audit-content/CTestDebugFinal.log), [format](../../../../out/audit-content/Style.log), [boundaries](../../../../out/audit-content/Boundaries.log), [naming](../../../../out/audit-content/Naming.log), [shader reproduction before](../../../../out/audit-content/ShaderAudit.log), [shader reproduction after](../../../../out/audit-content/ShaderAuditAfter.log), [publication reproduction after](../../../../out/audit-content/PublicationRaceAfter.log).

## Remaining limits

The first full Release run's startup-readiness failure remains recorded; two successful retries are not proof that every cold-start schedule fits the fixed warmup. The previously recorded warm-cache deferred descriptor capacity failure was not repaired in this migration audit. Its shader-loader control and limits remain in [verification.md](verification.md); the audit's first new-cache deferred run passed, which does not close that intermittent issue.

No complete historical checkout rebuild, remote LFS push/fresh clone, or full upstream redownload was performed. No findings required a module redesign, persistent protocol change or unrelated renderer lifecycle repair. Both repositories remain available for local acceptance without staging, commits or OpenSpec archival.
