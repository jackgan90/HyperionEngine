## Context

Assets.h currently owns FImage, EColorSpace and FImagePixels together with mesh types, reflection and codec/file functions. RHITypes.h includes that header and hyperion_rhi publicly links hyperion_assets. RHI uploads and swapchain readback need image values, not asset loading or codecs. Gui's font atlas and RenderOutput's public completion contract also consume the same values.

## Goals / Non-Goals

**Goals:** One standard-library-only image definition; a service-independent RHI dependency closure; explicit public dependencies; unchanged image, codec, upload and screenshot semantics.

**Non-Goals:** Geometry migration, texture pixel layout redesign, new readback formats, Platform extraction, CMake policy changes, provider replacement or new user capabilities.

## Decisions

- Add Runtime/ImageData with a header-only INTERFACE target and explicitly listed private header sources for graph/IDE ownership. This avoids an artificial implementation translation unit and avoids coupling raw images to Textures' reflected native asset schema. Public include and C++20 requirements use INTERFACE visibility.
- Move EColorSpace, FImage and FImagePixels verbatim. Width/height, enum values, default encoding, aggregate initialization, float precision and owned vector copy/move behavior remain unchanged. Assets.h re-exports the one definition for source compatibility. All decoding/encoding/IO stays in Assets.
- RHI includes ImageData directly and exports its target instead of Assets, with explicit Math for its FVec4 clear values. Gui replaces its image-only Assets include with ImageData and explicitly includes/exports Math for GUI vectors. RenderOutput includes ImageData; its implementation explicitly includes Assets for SaveImage. Renderer retains Assets for real native asset consumers and directly exports ImageData.
- D3D12 continues consuming images through its existing RHI public contract; it needs no separate direct edge unless it directly includes an ImageData header. Its own Math dependency remains explicit.
- Validate the actual transitive RHI closure against Assets, including intermediary links. Non-owned wrappers may link third-party targets but cannot link owned targets, including through generator expressions or graph properties; unsupported hiding fails collection with the declaration location. ImageData rejects every link dependency, including third-party libraries. Add a consumer linking only hyperion_rhi to prove upload/readback image signatures compile independently, and adversarial closure fixtures.

## Risks / Trade-offs

- Lost accidental includes → compile all owners and add explicit includes at the actual consumer; do not restore unrelated public exports.
- Interface source ownership → explicitly declare the header in add_library's private source list and inspect configured target metadata and VS generation.
- Changed image values → keep declarations and codecs unchanged; run existing PNG/EXR precision, texture, upload, readback and screenshot regressions in Debug/Release.

## Migration Plan

Introduce the data contract, migrate public includes and direct links, add architectural coverage and documentation, then build and test. Rollback restores the previous includes/links and removes the data module without asset/schema migration.

## Open Questions

None. Existing codec and screenshot protocols remain authoritative and unchanged.
