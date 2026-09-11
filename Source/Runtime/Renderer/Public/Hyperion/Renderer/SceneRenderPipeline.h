#pragma once
#include "Hyperion/Renderer/ForwardRenderPipeline.h"
#include "Hyperion/Renderer/FullscreenPass.h"

namespace Hyperion
{
enum class ESceneRenderPipeline : std::uint8_t
{
	Deferred,
	Forward
};

struct FGBufferLayout
{
	std::array<EMaterialColorFormat, 4> Formats{EMaterialColorFormat::Rgba8Unorm, EMaterialColorFormat::Rgba16Float,
	                                            EMaterialColorFormat::Rgba8Unorm, EMaterialColorFormat::Rgba16Float};
	static FGBufferLayout HighPrecision();
	void Validate(const FRHICapabilities& InCapabilities) const;
	std::uint32_t BytesPerPixel() const;
	bool operator==(const FGBufferLayout&) const = default;
};

struct FScenePipelineSettings
{
	ESceneRenderPipeline Pipeline = ESceneRenderPipeline::Deferred;
	FGBufferLayout GBuffer;
	float Exposure = 1;
	std::uint32_t DebugMode{};
};

// Main constructs; Render builds immutable frame descriptions. RHI retains submitted generations.
class FSceneRenderPipeline
{
public:
	FSceneRenderPipeline(FRenderSession& InSession, FRHICapabilities InCapabilities,
	                     FScenePipelineSettings InSettings = {});
	void Configure(FScenePipelineSettings InSettings);
	void Build(FRenderGraph& InGraph, FRenderView InMain, std::shared_ptr<const FMaterialFrameContext> InFrame,
	           const FCascadedShadowSettings& InShadows, FVec4 InClear,
	           const std::function<void(FRenderGraph&)>& InExtensions = {}, bool bInDeferPreparation = false);
	FForwardFrame GetFrame() const;
	const FCascadedShadowMap& Shadows() const;
	std::uint64_t TargetBytes() const;

private:
	struct FViewFamily
	{
		std::vector<FRenderView> Views;
		std::vector<FRenderPassTargets> Targets;
		std::size_t BaseIndex{};
		std::size_t TransparentIndex{};
	};

	FViewFamily MakeViews(FRenderView InMain, FVec4 InClear) const;
	FRenderSession& Session;
	FRHICapabilities Capabilities;
	FScenePipelineSettings Settings;
	FCascadedShadowMap ShadowMaps;
	std::shared_ptr<const void> Lifetime;
	std::shared_ptr<const void> ShadowLifetime;
	std::uint64_t ShadowBytes{};
	EDepthConvention ShadowDepthConvention = EDepthConvention::Standard;
	std::shared_ptr<const FMaterialTextureSource> SceneColor;
	std::shared_ptr<const FMaterialTextureSource> SceneDepth;
	std::array<std::shared_ptr<const FMaterialTextureSource>, 4> GBuffer;
	std::uint32_t Width{};
	std::uint32_t Height{};
	EDepthConvention DepthConvention = EDepthConvention::Standard;
	FForwardPipelineStatistics LastStatistics;
	std::shared_ptr<FFullscreenPreparationStatistics> FullscreenStatistics;
	bool bPending{};
	void ClearTargets(FRenderGraph& InGraph, FVec4 InClear) const;
	void Resize(std::uint32_t InWidth, std::uint32_t InHeight, EDepthConvention InConvention);
	FRenderPassTargets ColorTargets(std::string InName, EAttachmentLoad InLoad, FVec4 InClear = {}) const;
	FRenderDepthTarget DepthTarget(EAttachmentLoad InLoad) const;
	FFullscreenPassDesc Lighting(const FRenderView& InMain, const FMaterialFrameContext& InFrame, FVec4 InClear) const;
	FFullscreenPassDesc Debug(const FRenderView& InMain) const;
	FFullscreenPassDesc Tonemap(const FRenderView& InMain) const;
};
} // namespace Hyperion
