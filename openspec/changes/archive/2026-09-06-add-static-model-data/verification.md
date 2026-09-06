# Static Scene data: verification

Date: 2026-09-06. Environment: Windows x64, NVIDIA GeForce RTX 5080, D3D12 debug layer enabled; Visual Studio 2022 Community / MSVC 19.38, plus Ninja / MSVC 19.50 build and naming database.

## Evidence

Known parent/child transforms produce two instances with shared geometry and expected bounds. glTF sources preserve primitive/node/material/image data, matrix/TRS semantics, generated directions and native reflected round-trip. Cycle/reference failures are explicit. Scene depends on Math/Reflection, not RHI or Renderer.

Relevant tests: `static_model_data`, `async_gltf_import`, `model_rendering`, `dependency_boundaries`.

Implementation/test pointers: `Source/Runtime/Scene`, `Source/Tests/Scene/ModelTests.cpp`.

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
