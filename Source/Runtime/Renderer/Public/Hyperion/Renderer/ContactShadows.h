#pragma once
#include "Hyperion/Renderer/FullscreenPass.h"
#include "Hyperion/Renderer/HierarchicalDepth.h"
#include "Hyperion/Scene/LightShadows.h"

namespace Hyperion
{
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
