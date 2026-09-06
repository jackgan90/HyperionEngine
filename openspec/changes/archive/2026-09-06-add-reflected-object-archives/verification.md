# Reflected memory archives: verification

Date: 2026-09-06. Environment: Windows x64, NVIDIA GeForce RTX 5080, D3D12 debug layer enabled; Visual Studio 2022 Community / MSVC 19.38, plus Ninja / MSVC 19.50 build and naming database.

## Evidence

Nested records, vectors and fixed arrays round-trip; numeric bulk arrays retain element identity. Missing/unknown fields preserve defaults, future schemas and truncation fail before caller replacement. Native model validation rejects invalid references during decode. Existing JSON configuration tests remain compatible.

Relevant tests: `reflected_archives`, `configuration_plugins`, `static_model_data`, `async_gltf_import`.

Implementation/test pointers: `Source/Runtime/Reflection`, `Source/Runtime/Serialization`, `Source/Tests/Serialization/ArchiveTests.cpp`.

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
