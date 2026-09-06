# Asynchronous glTF import: verification

Date: 2026-09-06. Environment: Windows x64, NVIDIA GeForce RTX 5080, D3D12 debug layer enabled; Visual Studio 2022 Community / MSVC 19.38, plus Ninja / MSVC 19.50 build and naming database.

## Evidence

External glTF, embedded GLB, data URIs and PNG/JPEG load through the engine IO provider. Instrumented reads run on one IO thread; duplicate requests issue three reads for the external sample, share an immutable object and isolate cancellation. Sparse/interleaved/normalized attributes and strip/fan winding match known values. Missing dependencies, bad accessor ranges, cycles, lines, truncated GLB and required unsupported extensions fail. Native save/load and cache invalidation pass, glTF export rejects explicitly, and temporary service destruction drains producers.

Relevant tests: `async_gltf_import`, `model_fixtures`, `model_rendering`, `model_viewer_acceptance`.

Implementation/test pointers: `Source/Runtime/Assets/Private/AssetService.cpp`, `Source/Runtime/AssetImport`, `Source/Tests/Assets/ImportTests.cpp`.

## Reproduction

```powershell
.\tools\GenerateSolution.ps1 -Test -Configuration Debug
.\tools\GenerateSolution.ps1 -Test -Configuration Release
python tools/CheckBoundaries.py
python tools/CheckStyle.py --naming --build-dir out/build/debug
openspec validate --all --strict
```

Full-suite results and final logs are recorded in [AssetPipeline.md](../../../../docs/AssetPipeline.md) and [VisualStudio.md](../../../../docs/VisualStudio.md). Local generated logs live in `out/GltfVsDebugVerified.log`, `out/GltfVsReleaseVerified.log`, `out/GltfNamingVerified.log` and `out/GltfOpenSpec.log`.

Screenshots: `out/captures/model-viewer-Showcase.gltf.png` (model with UI), `model-viewer-Showcase.glb.png` (embedded asset without UI), `model-mask.png`, `model-alpha.png`, `model-orbit.png`. Visual inspection accompanies pixel assertions; generated fixtures establish the documented static subset, not universal glTF conformance.
