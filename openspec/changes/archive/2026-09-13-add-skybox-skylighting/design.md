## Context

Native textures currently contain RGBA8 2D mips. HDR render targets already exist, as do shared fullscreen triangles and HDR Forward/Deferred paths. Scene environment lights publish a constant ambient semantic. Asset imports track source bytes and publish pinned immutable dependencies. SceneViewer must let users replace native sky assets without restarting or losing persistence.

## Goals / Non-Goals

Goals: native HDR/Cube assets; consistent infinite sky imagery and diffuse/specular IBL; generation-safe asynchronous scene use; Forward/Deferred parity; GUI replacement and three redistributable reference environments; numerical and D3D12 validation.

Non-goals: realtime sky simulation, dynamic reflection capture, local probes, sky visibility/GI, sun extraction, automatic exposure, BC compression or cube arrays.

## Decisions

1. Extend FTextureAsset with an engine-owned format and 2D/Cube dimension. Keep the existing mip byte storage and add face data in a deterministic face-major layout; old records migrate to 2D RGBA8. Use RGBA16F for shipped skies and support RGBA32F where half would overflow. Validate each face/mip, complete chains, finite float values, sRGB restrictions and total budgets. This reuses native serialization and material texture sharing rather than a sky-only GPU path.
2. Add CPU Runtime/Environment for FSkyAsset, cubemap sampling/conversion, irradiance SH and GGX filtering. It depends on Math, Textures and AssetTypes, never RHI. AssetImport reads source bytes and calls Assets' private stb/TinyEXR adapters. Support 2:1 equirectangular linear-sRGB HDR/EXR, reject unsupported projections/invalid radiance explicitly. Source descriptors carry bake quality; importer revision/settings participate in invalidation.
3. FSkyAsset references a radiance texture and a GGX prefiltered texture, stores nine RGB irradiance SH coefficients and a bake convention version. The visual mip chain uses ordinary directional filtering; roughness mips are separate. Integrate SH using texel solid angles and cosine convolution (pi, 2pi/3, pi/4), evaluate irradiance then divide by pi once in diffuse. Reuse the same GGX roughness and Smith/Fresnel conventions as the builtin BRDF. A deterministic engine-shared BRDF LUT is independent of each sky. CPU bake loops are bounded, deterministic and offline.
4. Cube support propagates through DXIL/SPIR-V reflection, Materials value types/serialization, Renderer resource binding, RHI contracts and D3D12 six-slice uploads/cube SRVs. Resource dimensions are validated at binding; legacy enum values and shader blocks remain stable. Whole-resource graph tracking suffices because imported cubes are immutable sampled resources; no per-face render-target infrastructure is required.
5. Extend FSceneEnvironmentLight with ConstantColor/SkyAsset source, FAssetRef, explicit yaw, common radiance intensity and background visibility. Node position/orientation remains irrelevant; authored yaw rotates all sky evaluation consistently. Scene owns persistent requested state; Renderer owns resolved CPU/GPU resources. Unbound sessions get disabled neutral sky inputs. New scene-owned semantics use a separate versioned environment block; AmbientColor is zero in sky mode.
6. Resolve references asynchronously with cancellation/selection generation checks. Publish visual, SH and reflection data together; retained frames own immutable complete generations. Initial loading uses zero environment contribution, with readiness false. Replacement can retain the last complete sky until the new generation is ready; failure remains visible and never restores constant ambient implicitly. Snapshot preserves requested references, including pending/failed choices, without substituting an old loaded asset. Save As recursively rebases them.
7. Reuse the fullscreen triangle with a sky-specific vertex program/state. Load HDR color and depth; test at far depth with writes disabled. Insert after all opaque/compatibility geometry and before transparency, then apply the existing tonemap once. Direction reconstruction uses viewport and camera orientation without positional parallax. Normal and reflection directions transform by inverse sky yaw; camera motion does not rebake/upload environment data.
8. Centralize indirect lighting in common HLSL for Forward, Deferred, clustered-only and transparent PBR. Keep emissive/unlit independent. Existing GBuffer inputs suffice. Global IBL has no scene visibility or local reflection geometry; document this and avoid claiming GI or skylight shadows.
9. Ship three 1K/2K CC0 source environments with source URL, license and SHA-256 provenance. Build native products through AssetTool and expose shipped choices plus an editable arbitrary .hasset path and Apply button through FGui. Controls show pending/ready/error, active versus requested asset, source mode, sky visibility, yaw and intensity; saved scenes retain settings. Sponza selects the default sky and its previous fixed environment color becomes inactive. Existing direct lights remain independent and are calibrated as an authored combination.

## Risks / Trade-offs

- Global environment leaks into enclosed spaces -> document lack of visibility/GI and validate separately from sky background occlusion.
- Bright sun aliasing or half overflow -> solid-angle-aware integration, bounded importance filtering and explicit finite/range checks; choose storage precision without clamping source energy.
- CPU bake time and memory -> bounded quality settings, small shipped specular maps, incremental imports and no per-frame preprocessing.
- New asset completing after a newer selection -> compare requested reference/generation before publication and test rapid A/B/failure transitions.
- Legacy material resources/block ABI -> append enum values, preserve existing blocks, provide neutral environment defaults and run old material suites.

## Migration Plan

Old texture records default to RGBA8 2D and old lights to ConstantColor. Add explicit record migrations and bump affected importer revisions so cached parent graphs rebuild. Keep all sample source assets in assets and generated products in out. Validate existing scenes as well as the migrated Sponza. Changes remain uncommitted through review; archive and commit follow audit closure under the user's 2026-09-13 request.

## Open Questions

None blocking implementation. Exact sample quality and Sponza intensity will be chosen using measured import/runtime cost and actual rendered images; final values and limitations will be recorded in verification.
