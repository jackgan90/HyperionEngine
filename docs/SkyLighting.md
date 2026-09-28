# Skybox and skylighting

当前 Content 归属、目录选择及外部资产重建见 [Content 与虚拟文件系统](ContentFileSystem.md)。

The scene environment can use a native sky asset for an infinitely distant background and diffuse/specular image-based lighting. One linear HDR panorama produces the visual cube, nine irradiance SH coefficients and a separate GGX reflection cube. Yaw, intensity and tint apply to all three together. `ConstantColor` and `SkyAsset` are exclusive environment sources; directional, point and spot lights remain independent.

## Try it

From the repository root:

```powershell
./tools/Build.ps1
./out/build/debug/bin/hyperion_editor.exe --asset-root ../HyperionAssets --scene /Game/Scenes/Sponza.hasset
```

Environment lighting is authored as a **Sky Light** scene component. Drag **Sky Light** from Window > Place Object (Basic or Lights) into the viewport to create one referencing the Engine default Cloudy sky. The effectively enabled light with the highest signed integer **Priority** supplies the single global environment. Priority defaults to zero and permits negatives. Equal highest priorities resolve by lexical persistent object ID and show a contextual `[!]` warning on Priority. Hovering this marker explains that multiple enabled Sky Lights share the highest Priority; the adjacent `(?)` independently provides status and advice. Priority help shows **Current effective environment** in green for the winner, or **Environment overridden by <display name>** in red for another enabled sky, followed by advice to increase Priority above the other sky lights. Tooltip prose omits internal object IDs and tie-breaking details. Disabled skies retain their disabled explanation. Disabling or deleting the winner automatically resolves another candidate. Selection is derived, never a separate scene setting.

Details show only the fields that apply to the selected **Source**. Both sources share **Priority** and **Intensity**. **Constant color** uses **Color** times intensity as uniform ambient radiance. **Sky asset** shows the **Sky asset** picker, **Tint**, **Yaw (degrees)** and **Show background**. The picker accepts indexed or dragged native sky assets; other asset types are rejected. Tint multiplies the background, diffuse SH and specular IBL without affecting direct, local or emissive light. Tint, yaw and intensity update rendering parameters without reloading the sky. Turning off **Show background** retains skylighting. Values hidden by the current source remain stored. Ready state is silent; pending or failed status is attached to the Sky asset property, with dependency/upload errors in its tooltip. Save Scene persists Priority and other component properties; Save As rebases references.

Readiness, intensity and background visibility do not affect selection. A selected sky with no complete products contributes zero environment light; it never falls back to a lower-priority component. Replacement within the same component retains its previous complete sky during loading or failure; visual and lighting never mix generations. The GUI reports failure and preserves the requested reference. Applying the same failed path again starts a new attempt, including after a missing or wrong-type file has been corrected. Explicit unpinned paths read the current root file; pinned ID/revision references retain their immutable identity contract. Snapshots preserve pending/failed requests, rather than saving the old displayed sky in their place.

## Import a sky

```powershell
./out/build/debug/bin/hyperion_asset_tool.exe import ../HyperionAssets/.cache/Sources/Skies/Cloudy.hdr out/my-content/Skies/Cloudy.hasset --library out/my-content
./out/build/debug/bin/hyperion_asset_tool.exe import path/to/environment.exr out/my-content/Skies/Custom.hasset --library out/my-content
./out/build/debug/bin/hyperion_asset_tool.exe validate out/my-content/Skies/Custom.hasset
```

Import a 2:1 HDR/EXR panorama directly. The Import Asset panel exposes Radiance size, Specular size and Sample count and generates diffuse SH and the prefiltered specular cube. Sky JSON recipes and serialized sky JSON are not supported. Existing native sky assets remain loadable and editable; they are not import sources.

AssetTool accepts `--radiance-size 256 --specular-size 64 --samples 256`; automation uses the reflected `sky` settings object. Source bytes and settings participate in incremental import. Runtime scene loading consumes only `.hasset` files and their shared dependencies. Copy the sky and its referenced native textures together; catalog and import-library files are not required.

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

The common PBR indirect-light function is used by Forward, Deferred, clustered lighting and transparent materials. Metallic F0, roughness, normal maps and material occlusion feed the IBL evaluation. The legacy constant-color formula is retained in constant mode. No GBuffer attachment was added. Environment shader bindings occupy the separate versioned `EnvironmentV1` block in space 2; existing material/light blocks remain unchanged. Tint is premultiplied into the SH coefficients and carried for specular and background in their unused `w` lanes, so the block layout is unchanged. Material providers supply disabled neutral defaults to sessions without a sky.

The sky uses the shared fullscreen triangle and reconstructs world directions from projection and camera rotation. Translation is excluded before matrix inversion, so large camera positions cannot introduce cancellation errors in sky sampling. It writes into linear HDR color after opaque/compatibility rendering and before transparency, with far-depth testing and depth writes disabled. Standard and reversed-Z, sub-viewports and viewport depth ranges are supported. Sky and surfaces pass through the existing exposure/tonemap once. The render graph exposes `Scene/Sky`; Editor benchmark CSV includes `sky_gpu_ms` from existing GPU timestamps.

Fullscreen sampled resources follow asynchronous readiness, including neutral textures on first use in old scenes. A pass with an unfinished upload keeps its attachment load/clear behavior but submits no draw that frame; later frames retry using the same resource. This does not introduce a GPU wait or repeated upload. Actual sky generations are prepared before scene publication.

Scene's persistent `FSceneEnvironmentLight` v3 stores source, color, intensity, sky reference, tint, yaw in degrees and background visibility. v1 records migrate to ConstantColor; v2 yaw radians convert to degrees, and an absent v2 reference becomes the Engine default sky. Runtime prepared data is immutable and omitted from serialization. Renderer asynchronously resolves the complete dependency generation, prepares shared textures on RHI, publishes only after upload readiness, and rejects superseded selections. Close joins admitted work before resource teardown. Scene remains independent of Renderer/RHI.

## Sponza defaults and limits

Sponza now selects Cloudy at intensity 1 and yaw 0. Existing exposure 2.5 and authored direct lights are preserved. The former fixed environment color remains stored but inactive. Cloudy is the default because its broad sky contribution keeps the atrium readable with the existing camera and light rig; Dusk and Clear are alternative lighting conditions for inspection, not brightness-normalized variants.

This is global environment lighting without scene visibility. It does not add skylight shadows, indoor occlusion, bounced GI, local reflections, parallax correction or specular occlusion beyond existing material AO. The sky background is depth-occluded, but that depth test does not prevent environment light from reaching an enclosed surface. SH9 can smooth narrow bright features; specular quality is limited by the chosen cube size and sample count. Multiple-scattering energy compensation is not implemented.

Bright sun energy remains in the panorama. No sun is extracted, removed or automatically aligned to the authored directional light, so direct sun and IBL may overlap. Intensity uses relative source radiance; there is no absolute photometric calibration or automatic exposure. A physically matched sun/sky rig is a separate authoring task.

See [verification](../openspec/changes/archive/2026-09-13-add-skybox-skylighting/verification.md) for tested configurations, images and measured costs, and the [quality audit](../openspec/changes/archive/2026-09-13-add-skybox-skylighting/audit.md) for the reviewed fixes and final checks. The completed change was archived on 2026-09-13.
