#include "Hyperion/Renderer/RenderPass.h"
#include <array>

namespace Hyperion
{
bool FRenderColorTarget::operator==(const FRenderColorTarget& InOther) const
{
	return Source == InOther.Source && Actions == InOther.Actions && View == InOther.View &&
	       std::array{Clear.X, Clear.Y, Clear.Z, Clear.W} ==
	           std::array{InOther.Clear.X, InOther.Clear.Y, InOther.Clear.Z, InOther.Clear.W};
}

FRenderPassTargets FRenderPassTargets::ColorOnly(std::optional<FVec4> InClear)
{
	FRenderPassTargets Result;
	Result.Color = FRenderColorTarget{{ERenderTargetKind::Backbuffer},
	                                  {InClear ? EAttachmentLoad::Clear : EAttachmentLoad::Load},
	                                  InClear.value_or(FVec4{})};
	return Result;
}

FRenderPassTargets FRenderPassTargets::Frame(ERHIDepthFormat InDepth, std::optional<FVec4> InClear)
{
	auto Result = ColorOnly(InClear);
	if (InDepth != ERHIDepthFormat::None)
	{
		Result.DepthStencil =
		    FRenderDepthTarget{{ERenderTargetKind::FrameDepth}, InDepth, FAttachmentActions{EAttachmentLoad::Clear}};
		if (InDepth == ERHIDepthFormat::D32S8)
		{
			Result.DepthStencil->Stencil = FAttachmentActions{EAttachmentLoad::Clear};
		}
	}
	return Result;
}

ERHIDepthFormat FRenderPassTargets::GetDepthFormat() const
{
	return DepthStencil ? DepthStencil->Format : ERHIDepthFormat::None;
}

std::uint32_t FRenderPassTargets::ColorCount() const
{
	return Color ? 1U : 0U;
}
} // namespace Hyperion
