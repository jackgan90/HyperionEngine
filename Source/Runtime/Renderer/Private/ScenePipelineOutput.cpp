#include "Hyperion/Renderer/SceneRenderPipeline.h"

namespace Hyperion
{
void FSceneRenderPipeline::SetOutputTarget(FRenderTargetSource InTarget)
{
	if (InTarget.Kind == ERenderTargetKind::Texture)
	{
		const auto* Color = InTarget.Texture ? InTarget.Texture->GetColorTarget() : nullptr;
		if (!Color || !InTarget.Lifetime || Color->Format != EMaterialColorFormat::Rgba8Unorm)
		{
			throw std::invalid_argument("Scene output requires a retained RGBA8 color target");
		}
	}
	else if (InTarget.Kind != ERenderTargetKind::Backbuffer || InTarget.Texture || InTarget.Lifetime)
	{
		throw std::invalid_argument("Scene output must be a backbuffer or color texture");
	}
	OutputTarget = std::move(InTarget);
}

FRenderPassTargets FSceneRenderPipeline::OutputTargets(std::optional<FVec4> InClear) const
{
	auto Result = FRenderPassTargets::ColorOnly(InClear);
	Result.Color->Source = OutputTarget;
	Result.Color->View = EGraphColorView::Srgb;
	return Result;
}
} // namespace Hyperion
