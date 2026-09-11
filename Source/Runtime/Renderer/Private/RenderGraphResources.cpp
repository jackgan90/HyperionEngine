#include "RenderGraphResources.h"
#include <cmath>
#include <set>
#include <stdexcept>

namespace Hyperion
{
namespace
{
std::optional<FViewport> AttachmentRegion(const std::optional<FViewport>& InViewport)
{
	if (!InViewport)
	{
		return {};
	}
	const auto Left = std::floor(InViewport->X);
	const auto Top = std::floor(InViewport->Y);
	return FViewport{Left, Top, std::ceil(InViewport->X + InViewport->Width) - Left,
	                 std::ceil(InViewport->Y + InViewport->Height) - Top};
}
} // namespace

std::vector<FGraphResourceState> ResolveGraphResources(std::span<const FGraphTextureImport> InResources)
{
	std::vector<FGraphResourceState> Result;
	std::set<const IRHITexture*> Textures;
	for (const auto& Import : InResources)
	{
		auto Target = Import.Target;
		if (Import.Resolve)
		{
			Target.Texture = Import.Resolve();
			if (!Target.Texture)
			{
				throw std::invalid_argument("Graph import resolved an empty texture");
			}
		}
		if (Target.Kind == ERenderTargetKind::Texture)
		{
			const auto Info = Target.Texture.Payload->GetInfo();
			if (Info.Width != Import.Size.Width || Info.Height != Import.Size.Height ||
			    Info.DepthFormat != Import.DepthFormat ||
			    (Import.DepthFormat == ERHIDepthFormat::None &&
			     (!Info.bColorTarget || Info.ColorFormat != Import.ColorFormat)))
			{
				throw std::invalid_argument("Graph import differs from the physical texture description");
			}
		}
		if (Target.Kind == ERenderTargetKind::Texture && !Textures.insert(Target.Texture.Payload.get()).second)
		{
			throw std::invalid_argument("Different graph identities resolve to the same texture");
		}
		Result.push_back(
		    {Target, Import.InitialState, {Import.bInitialized}, {Import.bInitialized}, {Import.bInitialized}});
	}
	return Result;
}

void TransitionGraphResource(FGraphResourceState& InResource, EResourceState InState, FPassCommands& OutCommands)
{
	if (InResource.State != InState)
	{
		OutCommands.Transitions.push_back({InResource.Target, InResource.State, InState});
		InResource.State = InState;
	}
}

void ApplyGraphPass(const FGraphicsPass& InPass, std::span<const FGraphTextureImport> InResources,
                    std::vector<FGraphResourceState>& InStates, FPassCommands& OutCommands)
{
	OutCommands.Viewport = InPass.Viewport;
	const auto Region = AttachmentRegion(InPass.Viewport);
	for (const auto& Attachment : InPass.GetColors())
	{
		auto& State = InStates[Attachment.Texture.Index];
		const auto& Resource = InResources[Attachment.Texture.Index];
		State.Color.Load(Attachment.Actions.Load, Region, Resource.Size);
		TransitionGraphResource(State, EResourceState::RenderTarget, OutCommands);
		FColorAttachment Color{State.Target, Attachment.Actions, Attachment.Clear,
		                       Attachment.View == EGraphColorView::Srgb, Resource.ColorFormat};
		if (InPass.Color)
		{
			OutCommands.Color = Color;
		}
		else
		{
			OutCommands.Colors.push_back(Color);
		}
	}
	if (InPass.DepthStencil)
	{
		const auto& Attachment = *InPass.DepthStencil;
		const auto& Resource = InResources[Attachment.Texture.Index];
		auto& State = InStates[Attachment.Texture.Index];
		if (Attachment.Depth)
		{
			State.Depth.Load(Attachment.Depth->Load, Region, Resource.Size);
		}
		if (Attachment.Stencil)
		{
			State.Stencil.Load(Attachment.Stencil->Load, Region, Resource.Size);
		}
		TransitionGraphResource(State, EResourceState::DepthWrite, OutCommands);
		OutCommands.DepthStencil =
		    FDepthStencilAttachment{State.Target,       Resource.DepthFormat,  Attachment.Depth,
		                            Attachment.Stencil, Attachment.ClearDepth, Attachment.ClearStencil};
	}
	for (const auto Read : InPass.Reads)
	{
		auto& State = InStates[Read.Index];
		const auto& Content = InResources[Read.Index].DepthFormat == ERHIDepthFormat::None ? State.Color : State.Depth;
		if (!Content.Contains({}, InResources[Read.Index].Size))
		{
			throw std::invalid_argument("Graph samples undefined texture contents");
		}
		TransitionGraphResource(State, EResourceState::ShaderRead, OutCommands);
		OutCommands.SampledTextures.push_back(State.Target.Texture);
	}
}

void FinishGraphAttachments(const FGraphicsPass& InPass, std::span<const FGraphTextureImport> InResources,
                            std::vector<FGraphResourceState>& InStates)
{
	const auto Region = AttachmentRegion(InPass.Viewport);
	for (const auto& Color : InPass.GetColors())
	{
		const auto Index = Color.Texture.Index;
		InStates[Index].Color.Store(Color.Actions.Store, Region, InResources[Index].Size);
	}
	if (InPass.DepthStencil)
	{
		const auto& Attachment = *InPass.DepthStencil;
		const auto Index = Attachment.Texture.Index;
		if (Attachment.Depth)
		{
			InStates[Index].Depth.Store(Attachment.Depth->Store, Region, InResources[Index].Size);
		}
		if (Attachment.Stencil)
		{
			InStates[Index].Stencil.Store(Attachment.Stencil->Store, Region, InResources[Index].Size);
		}
	}
}
} // namespace Hyperion
