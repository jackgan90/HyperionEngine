# Verification

## Quality audit follow-up (2026-09-23)

Independent review and parent reproduction confirmed that `validate-library` incorrectly selected zero entries for mounted subdirectories after local paths were normalized. The repair keeps complete mount indexes for dependencies and discovers entries from the requested directory. Regression coverage checks valid and corrupt payloads through physical and logical `/Engine/Sub` and `/Game/Sub` paths, and rejects missing directories.

Debug and Release incremental builds passed. All 7 distinct selected audit tests passed in Debug (the new library fixture was isolated after an initial duplicate-ID test failure), and Release passed 7/7 in one run. The selection and the companion transport repair are detailed in `../add-engine-automation-foundation/verification.md`. Evidence lives under `out/QualityAudit/AutomationContentRoot-20260923-111809`; no commit or archive was created. This CPU-only repair did not require another GPU acceptance run.

Validated on 2026-09-23 in the uncommitted working tree based on `7c711cb1c261e72b7dbf26ef9abcca28b56316e4`, including the preceding uncommitted automation foundation. No commit or OpenSpec archive was created.

## Build and static checks

- Full Ninja/MSVC x64 Debug and Release builds passed; final incremental builds include current sources and updated CTest arguments.
- Both profiles used Triangle, ModelViewer, SceneViewer, DebugUI and RenderDoc ON, with Tracy OFF. GPU acceptance used D3D12 on the available RTX 5080.
- `python tools/CheckStyle.py`: all owned filenames, include casing and formatting passed.
- `CheckStyle.check_naming` passed for all 40 changed/new C++ translation units against the Debug compilation database.
- `python tools/CheckBoundaries.py`: passed, including the new CPU-only Content module's transitive dependency contract.
- Python syntax checks for tools and integration tests passed; all seven affected performance tool CLIs expose `--asset-root`.
- Relative links in the 25 changed documentation files and `git diff --check` passed.
- `openspec validate replace-content-mount-config --strict` passed.

## Regression selection

The latest Debug results passed all 34 distinct selected tests, including required fixtures. Release passed 34/34 with the same final selection.

```powershell
ctest --test-dir out/build/release -R '^(automation_.*|editor_asset_.*|editor_content.*|editor_state|editor_preferences|plugin_applications|configuration_plugins|plugin_runtime|native_asset_.*|native_publication_cli|shared_asset_publication|async_io|shaders|editor_acceptance|editor_placement|scene_viewer_acceptance|viewer_acceptance|offline_startup|depth_viewer_acceptance|cpu_frame_viewer|d3d12_triangle|d3d12_gui|window_lifecycle)$' --parallel 4 --output-on-failure
```

Debug used this selection without the last four additional Viewer tests first, then reran `automation_.*`, `native_publication_cli`, `depth_viewer_acceptance`, `scene_viewer_acceptance`, `viewer_acceptance`, `cpu_frame_viewer`, `d3d12_triangle`, `d3d12_gui` and `window_lifecycle` after the migration fixes below. The follow-up passed 14/14, including fixtures. This is affected regression coverage, not a claim of a full CTest run.

Covered contracts:

- Empty Game startup with usable root discovery and strict reflected root schemas; real single-shot CLI, JSONL and MCP processes.
- Set, same-root no-op, clear, read-only changes, stale requests and prepared candidates, invalid roots, dirty rejection and explicit discard.
- Deterministic busy rejection while opening, saving or rebuilding texture data; saves finish against the old directory before switching.
- Old document invalidation, all-participant preflight before any release, scoped participant removal and continued use of externally owned asset services after plugin shutdown.
- Editor empty first launch, valid preference restoration, invalid preference recovery, explicit directory override and actual GUI root transitions.
- Native local file workflows, local library validation, Engine content overrides, default Engine read-only, explicit authoring and independent Game read-only enforcement.
- Existing Editor asset workflows, scene/document editing, placement, plugin absence/failure, native publication, shader compilation, Viewer startup and rendering/frame-pipeline behavior.

## Migration findings resolved during validation

The first expanded Debug run found two old generated fixtures containing the previous `.assets/<id>-<revision>` storage layout and duplicate IDs. The affected Debug/Release scene, depth and CPU-frame output directories were preserved under `out/ContentRootLegacyFixtures-*` and `out/ContentRootLegacyCpuFixtures-*`; tests regenerated current-format fixtures. No source or project asset directory was removed.

The Viewer triangle test also exposed a remaining implicit Game dependency: its sample Shader lives at `/Game/Shaders/Triangle.hlsl`. Viewer tests, benchmark tools and current command examples now explicitly select the sample asset directory. No engine-side sibling-directory fallback was introduced. The failing Viewer cases subsequently passed in both Debug and Release.

## Delivery boundary

`ContentMounts.json`, the JSON loader, local override discovery and `--mounts` support are removed. The typed mounted filesystem remains. Editor, automation and startup directory arguments use Runtime/Content; future live content consumers must register lifecycle participants.

Root state is process-local. No connection to an already running Editor or cross-process preference synchronization was added. Root preparation currently scans synchronously. Development builds default Engine content to the source `Content` directory; relocated clients supply `--engine-content` or configure the same API programmatically.

Generated evidence remains under `out/ContentRoot*Build.log`, `out/ContentRoot*Tests.log`, `out/ContentRootNaming.log` and build-local CTest logs. All source, documentation and OpenSpec changes remain uncommitted and unarchived.

## Archive handoff (2026-09-23)

The statements above record the implementation and audit stages before archive authorization. The original independent reviewer verified both repairs and closed QA-01 and QA-02 with independent CPU regression coverage. The user then authorized archive and Git commit. This change was archived with its main specifications synchronized. The companion verification is now available in [the archived automation change](../2026-09-23-add-engine-automation-foundation/verification.md). No engine code changed during archive; generated audit evidence remains outside the committed file set.
