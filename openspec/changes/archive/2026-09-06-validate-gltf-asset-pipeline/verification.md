# Pipeline acceptance: verification

Date: 2026-09-06. Environment: Windows x64, NVIDIA GeForce RTX 5080, D3D12 debug layer enabled; Visual Studio 2022 Community / MSVC 19.38, plus Ninja / MSVC 19.50 build and naming database.

## Evidence

Offline generated fixtures, CLI process acceptance, deterministic GPU pixels, controlled delayed IO, cancellation/drain and failed-save recovery are integrated into CTest. Original triangle/config/GUI/backend tests continue to pass. Shared runtime DLL copying is now a single dependency target, resolving a parallel MSBuild copy race uncovered during repeated Release builds.

Relevant tests: All 23 CTest tests, style, boundary, naming and OpenSpec validation.

Implementation/test pointers: `Source/Tests/Integration/ModelAcceptance.py`, `Source/Tests/Renderer/ModelRenderTests.cpp`, `tools/GenerateModelFixtures.py`, `docs/AssetPipeline.md`, `cmake/Modules.cmake`.

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

## Independent review follow-up

An independent read-only reviewer was started without inherited conversation history. The implementing agent separately confirmed all six findings and an additional pre-validation sparse buffer range issue, then applied bounded fixes. The reviewer examined the resulting patch and found no newly introduced issue. No thread, lock, cross-frame reuse or state-machine redesign was added.

New regressions cover combined sparse/interleaved attributes, malformed physical ranges and sparse counts, reversed deep node chains, conservative bounds under rotation/negative scale, off-axis transparency pixels and rejected constant-buffer offsets. Three correctness regressions failed against the pre-fix implementation; `out/ModelReviewBeforeTests.log` records that run.

See [ModelViewerReview.md](../../../../docs/ModelViewerReview.md) for per-finding confirmation, fixes, limitations and the final Debug/Release test evidence. Final logs use the `out/ModelReview*` prefix; the earlier glTF logs above describe the initial implementation.
