# Model rendering and camera: verification

Date: 2026-09-06. Environment: Windows x64, NVIDIA GeForce RTX 5080, D3D12 debug layer enabled; Visual Studio 2022 Community / MSVC 19.38, plus Ninja / MSVC 19.50 build and naming database.

## Evidence

GPU readback verifies near-first depth occlusion; linear .25 red produces sRGB .537; half-transparent red over blue produces sRGB (.735, 0, .735). Mask holes reveal blue, UV1 selects expected sRGB texels, sRGB black/white mip averages to byte188, and mirrored/double-sided geometry remains visible. ModelViewer polls texture fences; gated physical reads do not stop frames or resize. Captured GUI input does not change the camera; wheel, Home and right drag do. Failure state and GPU resource shutdown complete with zero D3D12 validation errors.

Relevant tests: `model_rendering`, `model_viewer_acceptance`, `d3d12_triangle`, `d3d12_gui`, `window_lifecycle`, `d3d12_device_ownership`.

Implementation/test pointers: `Source/Runtime/Renderer/Private/ModelRenderer.cpp`, `Source/Backends/D3D12/Private/D3D12Uploads.cpp`, `Source/Plugins/ModelViewer`, `shaders/Model.hlsl`.

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
