#include "RenderPassDeclaration.h"
#include "Hyperion/Renderer/RenderMaterial.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "MaterialSharedBinding.h"
#include "RenderResourcesInternal.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
namespace
{
FGraphTexture ImportTarget(FRenderGraph& InGraph, const FRenderResourcePreparation& InPreparation,
                           const FRenderTargetSource& InSource, ERHIDepthFormat InFormat)
{
	if (InSource.Kind != ERenderTargetKind::Texture && (InSource.Texture || InSource.Lifetime))
	{
		throw std::invalid_argument("Frame target source cannot carry a texture or scope");
	}
	if (InSource.Kind == ERenderTargetKind::Backbuffer)
	{
		const auto Texture = InGraph.ImportBackbuffer();
		InGraph.Export(Texture, EResourceState::Present);
		return Texture;
	}
	if (InSource.Kind == ERenderTargetKind::FrameDepth)
	{
		return InGraph.ImportFrameDepth(InFormat);
	}
	if (InSource.Kind != ERenderTargetKind::Texture || !InSource.Texture || !InSource.Texture->IsRenderTarget() ||
	    !InSource.Lifetime)
	{
		throw std::invalid_argument("Render pass requires a live explicit target source");
	}
	const auto* Depth = InSource.Texture->GetDepthTarget();
	const auto* Color = InSource.Texture->GetColorTarget();
	if ((Depth && InFormat != ERHIDepthFormat::D32) || (Color && InFormat != ERHIDepthFormat::None))
	{
		throw std::invalid_argument("Render target source aspect differs from declaration");
	}
	const auto Texture = InGraph.Import({"Target " + std::to_string(InSource.Texture->GetIdentity()),
	                                     {ERenderTargetKind::Texture},
	                                     {Depth ? Depth->Width : Color->Width, Depth ? Depth->Height : Color->Height},
	                                     InFormat,
	                                     EResourceState::ShaderRead,
	                                     InSource.bInitialized,
	                                     InSource.Texture,
	                                     [InPreparation, InSource]
	                                     {
		                                     return InPreparation.ResolveTexture(InSource.Texture, InSource.Lifetime);
	                                     },
	                                     Color ? GetRenderColorFormat(Color->Format) : ERHIColorFormat::Rgba8Unorm});
	InGraph.Export(Texture, EResourceState::ShaderRead);
	return Texture;
}

void AddSampledValue(const FMaterialValue& InValue, const std::shared_ptr<const void>& InOwner,
                     std::set<const FMaterialTextureSource*>& InSeen, std::vector<FRenderTargetSource>& OutReads)
{
	if (InValue.Texture && InValue.Texture->IsRenderTarget() && InSeen.insert(InValue.Texture.get()).second)
	{
		OutReads.push_back({ERenderTargetKind::Texture, InValue.Texture, InOwner});
	}
	for (const auto& Element : InValue.Elements)
	{
		AddSampledValue(Element, InOwner, InSeen, OutReads);
	}
}

} // namespace

std::vector<FRenderTargetSource> CollectMaterialReads(const FRenderSceneSnapshot& InSnapshot)
{
	auto Reads = InSnapshot.Targets.Reads;
	std::set<const FMaterialTextureSource*> Seen;
	for (const auto& Read : Reads)
	{
		Seen.insert(Read.Texture.get());
	}
	std::set<const FMaterialValue*> SeenArrays;
	for (const auto& Item : InSnapshot.Items)
	{
		if (!Item.ResolvedParameters || !Item.State.Surface || !Item.PreparationError.empty())
		{
			continue;
		}
		std::shared_ptr<const FCompiledMaterialDefinition> Compiled;
		const auto* Pass = Item.SharedBinding ? Item.SharedBinding->Pass : nullptr;
		if (!Pass || Pass->Usage != InSnapshot.View.Usage)
		{
			Compiled = Item.State.Surface->GetCompiled();
			Pass = &Compiled->GetPass(InSnapshot.View.Usage);
		}
		for (const auto& Binding : Pass->Bindings)
		{
			if (!Binding.ResourceParameter)
			{
				continue;
			}
			const auto& Value = Item.GetMaterialValue(*Binding.ResourceParameter);
			if (Value && ((Value->Texture && Value->Texture->IsRenderTarget()) ||
			              (!Value->Elements.empty() && SeenArrays.insert(Value.get()).second)))
			{
				AddSampledValue(*Value, Item.State.Surface, Seen, Reads);
			}
		}
	}
	return Reads;
}

FTexture FRenderResourcePreparation::ResolveTexture(std::shared_ptr<const FMaterialTextureSource> InSource,
                                                    std::shared_ptr<const void> InLifetime) const
{
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::lock_guard Lock(Owner.Mutex);
	if (Owner.bClosed || !InLifetime)
	{
		throw std::logic_error("Closed resource preparation or expired target scope");
	}
	Owner.EnsureMaterialCaches();
	Owner.TrackScope(InLifetime);
	return Owner.MaterialGpu->GetTexture(std::move(InSource), {std::move(InLifetime)});
}

FGraphicsPass FRenderResourcePreparation::DeclarePass(FRenderGraph& InGraph,
                                                      const FRenderSceneSnapshot& InSnapshot) const
{
	Coordinator->Tasks.Require({EDomain::Render});
	return DeclarePass(InGraph, InSnapshot, CollectMaterialReads(InSnapshot));
}

FGraphicsPass FRenderResourcePreparation::DeclarePass(FRenderGraph& InGraph, const FRenderSceneSnapshot& InSnapshot,
                                                      std::span<const FRenderTargetSource> InReads) const
{
	Coordinator->Tasks.Require({EDomain::Render});
	const auto& Targets = InSnapshot.Targets;
	FGraphicsPass Pass;
	Pass.Name = Targets.Name.empty() ? "Scene " + std::to_string(InSnapshot.Frame ? InSnapshot.Frame->Session : 0) +
	                                       "/" + std::to_string(InSnapshot.Frame ? InSnapshot.Frame->Frame : 0) + "/" +
	                                       std::to_string(InSnapshot.Family) + "/" +
	                                       std::to_string(InSnapshot.View.Identity) + "/" + InSnapshot.View.Usage
	                                 : Targets.Name;
	Pass.Viewport = InSnapshot.View.Viewport;
	for (const auto& Color : Targets.GetColors())
	{
		FGraphColorAttachment Attachment{
		    ImportTarget(InGraph, *this, Color.Source, ERHIDepthFormat::None), Color.Actions, Color.Clear,
		    Color.Source.Kind == ERenderTargetKind::Texture && Color.View == EGraphColorView::DrawBatch
		        ? EGraphColorView::Linear
		        : Color.View};
		if (Targets.Color)
		{
			Pass.Color = Attachment;
		}
		else
		{
			Pass.Colors.push_back(Attachment);
		}
	}
	if (Targets.DepthStencil)
	{
		const auto& Depth = *Targets.DepthStencil;
		Pass.DepthStencil =
		    FGraphDepthStencilAttachment{ImportTarget(InGraph, *this, Depth.Source, Depth.Format), Depth.Depth,
		                                 Depth.Stencil, Depth.ClearDepth, Depth.ClearStencil};
	}
	for (const auto& Read : InReads)
	{
		const auto Texture =
		    ImportTarget(InGraph, *this, Read,
		                 Read.Texture && Read.Texture->GetDepthTarget() ? ERHIDepthFormat::D32 : ERHIDepthFormat::None);
		if (std::find(Pass.Reads.begin(), Pass.Reads.end(), Texture) == Pass.Reads.end())
		{
			Pass.Reads.push_back(Texture);
		}
	}
	return Pass;
}
} // namespace Hyperion
