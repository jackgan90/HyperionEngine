# IO thread and storage: verification

Date: 2026-09-06. Environment: Windows x64, NVIDIA GeForce RTX 5080, D3D12 debug layer enabled; Visual Studio 2022 Community / MSVC 19.38, plus Ninja / MSVC 19.50 build and naming database.

## Evidence

Dedicated `EDomain::Io` with stable existing domain ownership; engine byte IO with local/memory providers, Unicode paths, read limits, cancellation and atomic replacement. One-worker resumable waits complete without deadlock. Repeated `.hasset` saves exercise replacement through IO; Viewer failed-save acceptance verifies remaining writes drain.

Relevant tests: `async_io`, `tasks`.

Implementation/test pointers: `Source/Runtime/IO`, `Source/Runtime/Tasks/Public/Hyperion/Tasks/AsyncResult.h`, `Source/Tests/IO/IOTests.cpp`.

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
