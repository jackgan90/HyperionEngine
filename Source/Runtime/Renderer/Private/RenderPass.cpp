#include "Hyperion/Renderer/RenderPass.h"
#include <array>

namespace Hyperion
{
ERHIColorFormat GetRenderColorFormat(EMaterialColorFormat InFormat)
{
	switch (InFormat)
	{
		case EMaterialColorFormat::Rgba8Unorm:
			return ERHIColorFormat::Rgba8Unorm;
		case EMaterialColorFormat::Rgba16Float:
			return ERHIColorFormat::Rgba16Float;
		case EMaterialColorFormat::Rgba32Float:
			return ERHIColorFormat::Rgba32Float;
	}
	throw std::invalid_argument("Unsupported material color format");
}

std::span<const FRenderColorTarget> FRenderPassTargets::GetColors() const
{
	if (Color && !Colors.empty())
	{
		throw std::invalid_argument("Render pass cannot mix single and multiple color attachment storage");
	}
	return Color ? std::span<const FRenderColorTarget>(&*Color, 1) : std::span<const FRenderColorTarget>(Colors);
}

FGraphicsTarget FRenderPassTargets::GraphicsTarget(bool bInSrgb) const
{
	const auto Attachments = GetColors();
	if (Attachments.size() > MaximumColorTargets)
	{
		throw std::invalid_argument("Render pass exceeds MRT capacity");
	}
	FGraphicsTarget Result{bInSrgb, GetDepthFormat(), static_cast<std::uint32_t>(Attachments.size())};
	for (std::size_t Index = 0; Index < Attachments.size(); ++Index)
	{
		const auto& Source = Attachments[Index].Source;
		if (Source.Kind == ERenderTargetKind::Texture)
		{
			if (!Source.Texture || !Source.Texture->GetColorTarget())
			{
				throw std::invalid_argument("Color target requires a color texture source");
			}
			Result.ColorFormats[Index] = GetRenderColorFormat(Source.Texture->GetColorTarget()->Format);
		}
	}
	return Result;
}

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
	return static_cast<std::uint32_t>(GetColors().size());
}
} // namespace Hyperion
