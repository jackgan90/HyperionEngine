#pragma once
#include "Hyperion/Renderer/FullscreenPass.h"
#include "Hyperion/Renderer/HierarchicalDepth.h"

namespace Hyperion
{
struct FContactShadowSettings
{
	bool bEnabled{};
	float Length = .35f;
	float Thickness = .05f;
	float Bias = .003f;
	std::uint32_t Steps = 96;
	std::uint32_t DebugMode{}; // 0: shaded, 1: visibility mask, 2: HZB mip (explicit depth consumer).
	std::uint32_t PreviewMip = 4;
	void Validate() const;
};

struct FContactShadowInputs
{
	FHierarchicalDepthProduct Depth;
	FRenderTargetSource Normals;
	FRenderTargetSource Surface;
	FRenderTargetSource Mask;
	FRenderView View;
	FVec3 LightDirection;
	FContactShadowSettings Settings;
};

FFullscreenPassDesc MakeContactShadowPass(const FRenderGraph& InGraph, const FContactShadowInputs& InInputs);
FFullscreenPassDesc MakeScreenTexturePreview(FRenderTargetSource InSource, FViewport InViewport, std::uint32_t InMip,
                                             bool bInInvert);
} // namespace Hyperion
