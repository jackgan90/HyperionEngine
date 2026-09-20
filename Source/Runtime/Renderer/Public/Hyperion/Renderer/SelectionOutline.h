#pragma once
#include "Hyperion/Renderer/RenderFeatures.h"

namespace Hyperion
{
enum class EOutlineOverlapMode : std::uint8_t
{
	Union,
	PerObject
};

struct FSelectionOutlineSettings
{
	EOutlineOverlapMode Overlap = EOutlineOverlapMode::Union;
	FVec4 Color{1, .45f, .025f, 1}; // Linear display color, composed after exposure/tonemapping.
	float Width = 2;                // Physical viewport pixels.
	bool bSupersample{};
};

struct FSelectionOutlineRequest
{
	std::optional<FScenePublicationToken> Publication;
	std::vector<std::vector<FRenderPrimitiveHandle>> Objects;
	FSelectionOutlineSettings Settings;
};

// Render feature instances and their targets belong to one viewport. No editor dependency.
std::unique_ptr<IRenderFeature> MakeSelectionOutlineFeature(FRHICapabilities InCapabilities);

// Reusable screen-space passes. Inputs/outputs must be distinct retained color targets.
// InMask is binary coverage: 1 for covered samples, 0 outside. Supersampling is resolved by this pass.
void AddSilhouetteOutlinePass(FRenderSession& InSession, FRenderGraph& InGraph, FRenderTargetSource InMask,
                              FRenderTargetSource InOutput, FViewport InViewport, float InWidth, bool bInClear,
                              bool bInDeferPreparation = true, std::string InName = "Outline/Exterior");
void AddOutlineCompositePass(FRenderSession& InSession, FRenderGraph& InGraph, FRenderTargetSource InOutline,
                             FRenderTargetSource InOutput, FViewport InViewport, FVec4 InColor,
                             bool bInDeferPreparation = true, std::string InName = "Outline/Composite");
} // namespace Hyperion
