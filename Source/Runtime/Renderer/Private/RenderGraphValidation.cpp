#include "Hyperion/Math/Bounds.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include "RenderGraphResources.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace Hyperion
{
namespace
{
void Actions(const FAttachmentActions& InActions)
{
	if ((InActions.Load != EAttachmentLoad::Load && InActions.Load != EAttachmentLoad::Clear &&
	     InActions.Load != EAttachmentLoad::Discard) ||
	    (InActions.Store != EAttachmentStore::Store && InActions.Store != EAttachmentStore::Discard))
	{
		throw std::invalid_argument("Invalid graph attachment load/store");
	}
}

void Region(const FGraphicsPass& InPass, const FGraphTextureImport& InResource)
{
	if (InPass.Viewport && InResource.Size.Width && InResource.Size.Height)
	{
		ValidateViewport(*InPass.Viewport, InResource.Size);
	}
}
} // namespace

void ValidateGraphState(const FGraphTextureImport& InResource, EResourceState InState)
{
	const auto Kind = InResource.Target.Kind;
	const bool bColor = InResource.DepthFormat == ERHIDepthFormat::None;
	const bool bValid =
	    Kind == ERenderTargetKind::Backbuffer
	        ? InState == EResourceState::Present || InState == EResourceState::RenderTarget
	        : (bColor ? InState == EResourceState::RenderTarget : InState == EResourceState::DepthWrite) ||
	              (Kind == ERenderTargetKind::Texture && InState == EResourceState::ShaderRead);
	if (!bValid)
	{
		throw std::invalid_argument("Resource state is incompatible with graph texture");
	}
}

void ValidateGraphImport(const FGraphTextureImport& InResource)
{
	if (InResource.Name.empty() || bool(InResource.Size.Width) != bool(InResource.Size.Height))
	{
		throw std::invalid_argument("Invalid graph resource name or dimensions");
	}
	const auto Kind = InResource.Target.Kind;
	if (Kind == ERenderTargetKind::Backbuffer)
	{
		if (InResource.DepthFormat != ERHIDepthFormat::None)
		{
			throw std::invalid_argument("Backbuffer cannot have a depth format");
		}
	}
	else if (Kind == ERenderTargetKind::FrameDepth)
	{
		if (InResource.DepthFormat != ERHIDepthFormat::D32 && InResource.DepthFormat != ERHIDepthFormat::D32S8)
		{
			throw std::invalid_argument("Unsupported frame depth format");
		}
	}
	else if (Kind == ERenderTargetKind::Texture)
	{
		if ((InResource.DepthFormat != ERHIDepthFormat::D32 && InResource.DepthFormat != ERHIDepthFormat::None) ||
		    InResource.ColorFormat >= ERHIColorFormat::Count || !InResource.Size.Width ||
		    (!InResource.Target.Texture && (!InResource.Identity || !InResource.Resolve)) ||
		    (InResource.Target.Texture && InResource.Resolve))
		{
			throw std::invalid_argument(
			    "Sampled target import requires explicit dimensions, supported format and one resource source");
		}
	}
	else
	{
		throw std::invalid_argument("Invalid graph resource target kind");
	}
	if (Kind != ERenderTargetKind::Texture && (InResource.Target.Texture || InResource.Identity || InResource.Resolve))
	{
		throw std::invalid_argument("Frame resource cannot carry texture resolution data");
	}
	ValidateGraphState(InResource, InResource.InitialState);
}

void ValidateGraphPass(const FGraphicsPass& InPass, std::span<const FGraphTextureImport> InResources,
                       std::uint64_t InGraph)
{
	if (InPass.Viewport)
	{
		constexpr auto Limit = std::numeric_limits<std::uint32_t>::max();
		ValidateViewport(*InPass.Viewport, {Limit, Limit});
	}
	const auto Resource = [&](FGraphTexture InTexture) -> const FGraphTextureImport&
	{
		if (InTexture.Graph != InGraph || InTexture.Index >= InResources.size())
		{
			throw std::invalid_argument("Foreign or stale graph attachment handle");
		}
		return InResources[InTexture.Index];
	};
	const auto Colors = InPass.GetColors();
	if (Colors.size() > MaximumColorTargets)
	{
		throw std::invalid_argument("Graph exceeds color attachment capacity");
	}
	std::set<std::size_t> ColorIndices;
	FSize ColorSize{};
	for (const auto& Attachment : Colors)
	{
		const auto& Color = Resource(Attachment.Texture);
		if ((Color.Target.Kind != ERenderTargetKind::Backbuffer && Color.Target.Kind != ERenderTargetKind::Texture) ||
		    Color.DepthFormat != ERHIDepthFormat::None || !ColorIndices.insert(Attachment.Texture.Index).second ||
		    !IsFinite(FVec3{Attachment.Clear.X, Attachment.Clear.Y, Attachment.Clear.Z}) ||
		    !std::isfinite(Attachment.Clear.W) ||
		    (Attachment.View != EGraphColorView::Linear && Attachment.View != EGraphColorView::Srgb &&
		     Attachment.View != EGraphColorView::DrawBatch) ||
		    (Color.Target.Kind == ERenderTargetKind::Texture && Attachment.View != EGraphColorView::Linear))
		{
			throw std::invalid_argument("Invalid graph color attachment, view or duplicate target");
		}
		if (ColorSize.Width && Color.Size.Width &&
		    (ColorSize.Width != Color.Size.Width || ColorSize.Height != Color.Size.Height))
		{
			throw std::invalid_argument("Graph color attachment dimensions differ");
		}
		if (Color.Size.Width)
		{
			ColorSize = Color.Size;
		}
		Actions(Attachment.Actions);
		Region(InPass, Color);
	}
	if (InPass.DepthStencil)
	{
		const auto& Attachment = *InPass.DepthStencil;
		const auto& Depth = Resource(Attachment.Texture);
		if (Depth.DepthFormat == ERHIDepthFormat::None || (!Attachment.Depth && !Attachment.Stencil) ||
		    (Attachment.Stencil && Depth.DepthFormat != ERHIDepthFormat::D32S8) ||
		    !std::isfinite(Attachment.ClearDepth) || Attachment.ClearDepth < 0 || Attachment.ClearDepth > 1)
		{
			throw std::invalid_argument("Invalid graph depth/stencil attachment");
		}
		for (const auto* Operation : {&Attachment.Depth, &Attachment.Stencil})
		{
			if (*Operation)
			{
				Actions(**Operation);
			}
		}
		Region(InPass, Depth);
		if (!Colors.empty())
		{
			const auto Size = ColorSize;
			if (Size.Width && Depth.Size.Width && (Size.Width != Depth.Size.Width || Size.Height != Depth.Size.Height))
			{
				throw std::invalid_argument("Graph color/depth dimensions differ");
			}
		}
	}
	for (const auto Read : InPass.Reads)
	{
		if (Resource(Read).Target.Kind != ERenderTargetKind::Texture || ColorIndices.contains(Read.Index) ||
		    (InPass.DepthStencil && InPass.DepthStencil->Texture == Read))
		{
			throw std::invalid_argument("Invalid or simultaneously writable graph sampled texture");
		}
	}
}
} // namespace Hyperion
