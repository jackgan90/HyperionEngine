# Skybox and skylighting

当前 Content 归属、目录选择及外部资产重建见 [Content 与虚拟文件系统](ContentFileSystem.md)。

The scene environment can use a native sky asset for an infinitely distant background and diffuse/specular image-based lighting. One linear HDR panorama produces the visual cube, nine irradiance SH coefficients and a separate GGX reflection cube. Rotation and intensity apply to all three together. `ConstantColor` and `SkyAsset` are exclusive environment sources; directional, point and spot lights remain independent.

## Try it

From the repository root:

```powershell
./tools/Build.ps1
./out/build/debug/bin/hyperion_viewer.exe --asset-root ../HyperionAssets --config experiments/Scene.json
```

The Scene panel is visible initially; Tab toggles panels. Its sky controls offer `Cloudy.hasset`, `Dusk.hasset` and `Clear.hasset` from `/Game/Skies`. Selecting an entry applies it immediately. Alternatively type an arbitrary native sky path and press **Apply sky asset**. Use virtual package paths for content selection; explicit local tool paths remain supported. **Refresh sky assets** uses the mounted filesystem to rescan the sibling `Skies` directory of the scene's containing directory. An empty custom path cannot be applied.

The controls show source mode, shared intensity, yaw in radians, background visibility, loading/uploading/ready/error status and the active sky name. While a different generation is pending or failed, they also show its requested path. The dropdown displays the active name even when the saved scene refers to an immutable library object rather than a shipped alias. Turning off **Show sky background** retains skylighting. Choosing **Constant color** restores the stored color multiplied by intensity. Save edited scene and `--save-scene` persist the sky reference and controls; Save As rebases relative references.

Initial loading contributes zero environment light until all products are ready. Replacement retains the previous complete sky during loading or failure; visual and lighting never mix generations. The GUI reports failure and preserves the requested reference so it can be corrected. Applying the same failed path again starts a new attempt, including after a missing or wrong-type file has been corrected. Explicit unpinned path selections read the current root file; pinned ID/revision references retain their immutable identity contract. It does not silently revert to constant ambient. Snapshots also preserve a pending/failed request, rather than saving the old displayed sky in its place.

## Import a sky

```powershell
./out/build/debug/bin/hyperion_asset_tool.exe import ../HyperionAssets/.cache/Sources/Skies/Cloudy.json out/my-content/Skies/Cloudy.hasset --library out/my-content
./out/build/debug/bin/hyperion_asset_tool.exe import path/to/environment.exr out/my-content/Skies/Custom.hasset --library out/my-content
./out/build/debug/bin/hyperion_asset_tool.exe validate out/my-content/Skies/Custom.hasset
```

HDR/EXR can be imported directly with default settings, or through a tracked source descriptor:

```json
{
  "type": "hyperion.skyasset",
  "schema_version": 1,
  "name": "My environment",
  "source": "environment.hdr",
  "radiance_size": 256,
  "specular_size": 64,
  "samples": 256
}
```

`source` resolves relative to the descriptor. Source bytes, settings and importer revision participate in incremental import. Runtime Viewer loads only `.hasset` files and their current shared dependencies; it does not link AssetImport or read HDR/EXR sources. Asset discovery rebuilds the registry from native metadata. Copy the sky and its referenced native textures together; catalog and import-library files are not required.

Input is a 2:1 equirectangular Radiance `.hdr` or ordinary RGB OpenEXR image (optional alpha). Decoding uses the existing private stb_image/TinyEXR adapters. RGB values are scene-linear, assumed to use linear sRGB/Rec.709 primaries; EXR chromaticities, arbitrary layered channels, multipart/deep images, camera exposure metadata and other panorama projections are not color-managed or converted. Alpha does not control sky coverage. Negative, non-finite or RGB values above `1e20` are rejected, rather than tone-mapped into the bake. Encoded input is bounded to 256 MiB and decoded RGBA to 512 MiB.

Radiance and specular face sizes must be powers of two, at most 1024 and 256 respectively; specular cannot exceed radiance. Samples must be 1–1024. Shipped defaults are 256/64 faces with 256 GGX samples, using 1K source images. Import processing is deterministic CPU work, bounded and offline. Increasing quality costs import time, storage and texture memory, with no per-frame convolution. Cancellation is checked around the bounded bake; it is not instantaneous within a bake loop.

The three unmodified source images are CC0. Asset pages, original download URLs and SHA-256 checksums are recorded in `../HyperionAssets/.cache/Sources/Skies/License.md`（见 HyperionAssets 的 Metadata 与本地源缓存）. Poly Haven website code is not incorporated.

## Resource and shading contracts

| Product | Role |
| --- | --- |
| `FSkyAsset`, `hyperion.skyasset` v1 | Typed references to radiance cube, GGX cube and shared BRDF LUT; irradiance SH9; bake convention v1 |
| Radiance cube | Visual sky with ordinary directional mips; common source for SH and GGX processing |
| Irradiance SH9 | Real SH integrated with cube texel solid angles and cosine convolution; shader evaluates irradiance and divides by pi once for Lambert diffuse |
| Specular cube | GGX importance-filtered levels; shader LOD is `roughness * (mipCount - 1)` |
| Shared BRDF LUT | 128×128 split-sum integration for the engine's separable Smith GGX visibility and Schlick Fresnel; shared independently of sky |

The visual and reflection cubes are distinct derived textures. Ordinary image mip generation is not roughness convolution. The shared Runtime/Environment module has no RHI dependency and exposes `PrefilterEnvironment` for future captured reflection cubes, plus panorama conversion, cube sampling and SH projection. Probe capture, placement and blending remain separate future work.

Native `FTextureAsset` v2 adds 2D/Cube dimension and RGBA8/RGBA16F/RGBA32F format. Old v1 records migrate to 2D RGBA8. Each mip stores tightly packed face planes in `+X, -X, +Y, -Y, +Z, -Z` order; all faces of one mip precede the next mip. Floating channels are little-endian, linear only. Cubes are square and require complete chains through 1×1. The importer selects RGBA32F if any source value exceeds 65000, preserving values that would overflow half. Existing material enum values remain stable, with TextureCube appended and actual resource dimensions checked during binding.

The D3D12 backend uploads each face/mip through the existing asynchronous upload/fence path and exposes a cube SRV. There are no sky-specific native texture handles or frame-time uploads. A default half-float sky's texture payload is about 4.42 MiB including the shared LUT, before GPU allocation alignment. The normal shared resource cache controls residency; selecting one sky does not require uploading every shipped sky.

The common PBR indirect-light function is used by Forward, Deferred, clustered lighting and transparent materials. Metallic F0, roughness, normal maps and material occlusion feed the IBL evaluation. The legacy constant-color formula is retained in constant mode. No GBuffer attachment was added. Environment shader bindings occupy the separate versioned `EnvironmentV1` block in space 2; existing material/light blocks remain unchanged. Material providers supply disabled neutral defaults to sessions without a sky.

The sky uses the shared fullscreen triangle and reconstructs world directions from projection and camera rotation. Translation is excluded before matrix inversion, so large camera positions cannot introduce cancellation errors in sky sampling. It writes into linear HDR color after opaque/compatibility rendering and before transparency, with far-depth testing and depth writes disabled. Standard and reversed-Z, sub-viewports and viewport depth ranges are supported. Sky and surfaces pass through the existing exposure/tonemap once. The render graph exposes `Scene/Sky`; Viewer benchmark CSV includes `sky_gpu_ms` from existing GPU timestamps.

Fullscreen sampled resources follow asynchronous readiness, including neutral textures on first use in old scenes. A pass with an unfinished upload keeps its attachment load/clear behavior but submits no draw that frame; later frames retry using the same resource. This does not introduce a GPU wait or repeated upload. Actual sky generations are prepared before scene publication.

Scene's persistent `FSceneEnvironmentLight` v2 stores source, reference, yaw, intensity and visibility; old records migrate to ConstantColor. Runtime prepared data is immutable and omitted from serialization. Renderer asynchronously resolves the complete dependency generation, prepares shared textures on RHI, publishes only after upload readiness, and rejects superseded selections. Close joins admitted work before resource teardown. Scene remains independent of Renderer/RHI.

## Sponza defaults and limits

Sponza now selects Cloudy at intensity 1 and yaw 0. Existing exposure 2.5 and authored direct lights are preserved. The former fixed environment color remains stored but inactive. Cloudy is the default because its broad sky contribution keeps the atrium readable with the existing camera and light rig; Dusk and Clear are alternative lighting conditions for inspection, not brightness-normalized variants.

This is global environment lighting without scene visibility. It does not add skylight shadows, indoor occlusion, bounced GI, local reflections, parallax correction or specular occlusion beyond existing material AO. The sky background is depth-occluded, but that depth test does not prevent environment light from reaching an enclosed surface. SH9 can smooth narrow bright features; specular quality is limited by the chosen cube size and sample count. Multiple-scattering energy compensation is not implemented.

Bright sun energy remains in the panorama. No sun is extracted, removed or automatically aligned to the authored directional light, so direct sun and IBL may overlap. Intensity uses relative source radiance; there is no absolute photometric calibration or automatic exposure. A physically matched sun/sky rig is a separate authoring task.

See [verification](../openspec/changes/archive/2026-09-13-add-skybox-skylighting/verification.md) for tested configurations, images and measured costs, and the [quality audit](../openspec/changes/archive/2026-09-13-add-skybox-skylighting/audit.md) for the reviewed fixes and final checks. The completed change was archived on 2026-09-13.
