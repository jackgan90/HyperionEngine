# Skybox / skylighting verification

Validated on 2026-09-13, Windows, NVIDIA GeForce RTX 5080, D3D12 debug layer enabled. Implementation and validation were completed before the separately requested archive and commit. Generated native content, builds and test outputs remain under ignored `out/`. Usage, resource contracts and limitations are documented in [SkyLighting.md](../../../../docs/SkyLighting.md).

Subsequent independent quality audit confirmed and repaired three edge cases in source switching, same-path failure retry and camera-translation precision. Fresh post-fix builds/tests and the separate review disposition are recorded in [audit.md](audit.md). The implementation measurements below retain their original validation boundary.

## Automated coverage

| Check | Result / evidence under `out/SkyImplementation` |
| --- | --- |
| Complete Debug build | Passed, `DebugBuildDelivery.log`; final scene test fixture rebuilt in `SceneTestBuildDebug.log` |
| Complete Release build | Passed, `ReleaseBuildDelivery.log`; final scene test fixture rebuilt in `SceneTestBuildRelease.log` |
| Debug regression | Full run 65/66 before fixing a cache-test ownership precondition, `DebugTestsComplete.log`; final affected suites 7/7 passed, `DeliveryFocusedTests.log` |
| Release regression | Complete final run 65/65 passed in 156.18 s, `ReleaseTestsComplete.log`; Release omits the Debug-only dispatch-failure injection suite |
| Code style | 499 owned source files passed, `StyleComplete.log` |
| Module boundaries | 470 source files / 29 modules passed, `BoundariesComplete.log` |
| Semantic naming | 307 translation units passed, `NamingComplete.log`; the final two edited units also passed `NamingLastEdits.log` |
| OpenSpec / whitespace | Strict validation and `git diff --check` passed, `OpenSpecFinal.log`, `DiffCheck.log` |

`environment_preprocessing` verifies actual shipped Radiance HDR and EXR decode, radiance values above 1, half/float storage, v1 texture migration, invalid cube rejection, axis/seam mapping, SH irradiance and roughness filtering, shared native dependency graphs and incremental import. For a constant test environment, SH error is below 0.002 irradiance units and all GGX levels preserve radiance within 0.002. Directional panorama reconstruction is checked within 0.025 per channel.

`sky_rendering` exercises real D3D12 RGBA16F and RGBA32F cubes, six faces, common yaw convention, camera translation without parallax, sub-viewports/depth ranges, standard and reversed-Z, diffuse and metallic surfaces, synthetic roughness-level colors, hidden background with retained illumination, transparency, clustered-only and clustered-plus-directional paths, nonclustered Deferred local lights and clustered Forward. Observed image differences between equivalent lighting paths are at most approximately 1/255, within the existing 0.02 test tolerance for GBuffer/output quantization. Pure sky direction comparisons are identical.

The same GPU suite covers missing references, keeping the last complete sky, rapid A/B replacement, saving a failed requested reference, native Save As/reload, closing admitted loads and eight actual camera translations without any increase in texture-upload count. Shaders are also compiled/reflected through DXIL, SPIR-V and MSL tests.

`sky_gui_controls` feeds real FGui mouse, keyboard and text events at 2× framebuffer scale. It verifies a disabled empty Apply button, typed custom path followed by Apply, shipped dropdown selection, source switching to sky, visibility and serialized controls. The SceneViewer integration uses this same control implementation. The final GUI screenshot was inspected for readable labels and an identifiable active sky.

Two verification issues were corrected without weakening assertions:

- First-use neutral environment textures exposed a fullscreen preparation assumption that every sampled resource was already uploaded. Fullscreen preparation now returns an empty draw batch while upload is pending, preserving attachment actions and retrying later. `CheckFullscreenPendingUpload` deterministically holds upload completion, verifies no draw/no repeated upload/no GPU wait, then permits exactly one draw. Old Viewer, offline startup, frame-pipeline and RenderDoc cases passed after this fix.
- `CheckSharedMaterialPublication` asserted exact descriptor allocation counts after releasing all old cache owners. The fixture now explicitly retains the previous collected items through the numeric edit, then releases them before retirement checks. The original no-new-set assertion is unchanged. Production cache lifetime rules were not weakened.

The scene importer revision expectations were updated to version 4, and repeated environment-import tests accept a verified up-to-date result. An intermediate link attempt was blocked by a test process holding the executable; final builds were performed after that process exited. Earlier failing logs are retained beside final results.

## Shipped Sponza environments and GUI

Each source was imported through AssetTool into native content. Each native Sponza variant ran for 1200 frames, saved to another native scene path and was reloaded for another 1200 frames. All runs ended with 1/1 models ready, 0 failed, 79 scene draws and zero D3D12 validation errors. Every saved/reloaded PNG is byte-for-byte identical to its corresponding original. See `ImageAcceptance.json` and the six image-run logs.

| Environment | Capture | Reload |
| --- | --- | --- |
| Cloudy daylight, `.hdr` | [Cloudy](../../../../out/SkyImplementation/Cloudy.png) | `CloudyReload.png`, identical |
| Dusk, `.exr` | [Dusk](../../../../out/SkyImplementation/Dusk.png) | `DuskReload.png`, identical |
| Clear daylight, `.hdr` | [Clear](../../../../out/SkyImplementation/Clear.png) | `ClearReload.png`, identical |

The images visibly differ in sky color, surface illumination and reflection. Input panoramas are not normalized to the same brightness. Sponza uses Cloudy with sky intensity 1, yaw 0, exposure 2.5 and its existing authored directional/point lights. This is an authored combination, not a physically calibrated sun/sky reconstruction.

The final default UI capture is [SkyGuiFinal.png](../../../../out/SkyImplementation/SkyGuiFinal.png), verified with `--verify-ui --verify-model`. It displays the active sky name, all sky control labels and ready status; switching uses the shipped `.hasset` list or a custom path. Sources, licensing and hashes are recorded in [License.md](../../../../assets/Skies/License.md).

## Static and moving camera measurement

Release, 1440×728, Deferred + clustered lighting, reversed-Z, CSM, Cloudy, GUI off, VSync off, 4 workers and 2 RHI threads. Each isolated run used 6000 frames with 4000 warmup frames and 2000 measured frames. The moving case used existing `--benchmark-camera --benchmark-camera-step 0.1` to exercise a small back-and-forth orbit. No other build or GPU test ran concurrently. Scene readiness is checked before benchmark admission; all sampled frames had 79 scene draws and zero failed items.

| Metric | Static | Moving |
| --- | ---: | ---: |
| Mean main frame interval | 6.1163 ms | 8.3829 ms |
| Mean pipeline preparation | 0.7511 ms | 3.1532 ms |
| Mean sky GPU pass | 0.003812 ms | 0.003886 ms |
| Mean lighting GPU pass | 0.06104 ms | 0.06114 ms |
| Mean sum of timed GPU passes | 0.28044 ms | 0.29051 ms |
| Descriptor allocation count | 392 throughout | 392 throughout |
| Pipeline creation count | 7 throughout | 7 throughout |
| Total GPU allocation bytes, min–max | 526606336–526606336 | 526606336–526802944 |

The moving run's total allocation footprint varied by 192 KiB; this measurement does not attribute that variation to a specific allocator. Texture upload stability is separately asserted by the sky GPU test. `lighting_gpu_ms` includes the existing directional/local lighting work plus IBL and therefore does not isolate the incremental IBL cost. These are measurements of this scene/device, not a general renderer speed claim. Raw CSV/logs and min/max/means are `Static.csv`, `Moving.csv`, `Static.log`, `Moving.log` and `BenchmarkSummary.json`.

## Deliberate boundaries

Global SH/GGX environment lighting has no scene visibility, skylight shadows, local probes, parallax correction, reflection capture, multiple-scattering compensation or bounced GI. Material AO is reused. Background depth occlusion does not solve lighting leakage into enclosed spaces. SH9 and the default 64-face specular map are quality approximations. Sky sun energy is retained; the authored directional sun is not extracted or automatically matched. Input RGB primaries are assumed linear sRGB/Rec.709, without metadata-based color/exposure conversion. These limits are documented and are not represented as implemented features.
