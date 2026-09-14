#include "Hyperion/Renderer/HierarchicalDepth.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include "Hyperion/Renderer/RenderSession.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace Hyperion
{
namespace
{
FHierarchicalDepthProduct CreateProduct(FRenderSession& InSession, const FHierarchicalDepthRequest& InRequest)
{
	const auto& Depth = *InRequest.Depth.Texture->GetDepthTarget();
	FHierarchicalDepthProduct Product;
	Product.Convention = InRequest.View.DepthConvention;
	Product.Reduction = InRequest.Reduction;
	const auto Mips = static_cast<std::uint32_t>(std::bit_width(std::max(Depth.Width, Depth.Height)));
	Product.Texture =
	    std::make_shared<const FMaterialTextureSource>(FMaterialStorageTexture{Depth.Width, Depth.Height, Mips});
	Product.Lifetime = InSession.GetResources().CreateScopeLifetime();
	for (std::uint32_t Mip = 0; Mip < Mips; ++Mip)
	{
		const FSize Size{std::max(1U, Depth.Width >> Mip), std::max(1U, Depth.Height >> Mip)};
		Product.MipSizes.push_back(Size);
		Product.Bytes += std::uint64_t(Size.Width) * Size.Height * 4;
	}
	return Product;
}

void Generate(FRenderSession& InSession, FRenderGraph& InGraph, const FHierarchicalDepthRequest& InRequest,
              const FHierarchicalDepthProduct& InProduct, bool bInDeferPreparation)
{
	const auto View =
	    InRequest.View.Viewport.value_or(FViewport{0, 0, float(InRequest.View.Width), float(InRequest.View.Height)});
	for (std::uint32_t Mip = 0; Mip < InProduct.MipSizes.size(); ++Mip)
	{
		const auto Size = InProduct.MipSizes[Mip];
		FComputePassDesc Pass;
		Pass.Name = "HZB/" + std::to_string(InRequest.View.Identity) + "/" +
		            std::to_string(InRequest.Depth.Texture->GetIdentity()) + "/" +
		            (InRequest.Reduction == EDepthReduction::Nearest ? "Nearest/" : "Farthest/") + std::to_string(Mip);
		Pass.Shader.Source = Mip == 0 ? "Depth/HierarchicalCopy.hlsl" : "Depth/HierarchicalReduce.hlsl";
		Pass.Lifetime = InProduct.Lifetime;
		Pass.Extent = {Size.Width, Size.Height, 1};
		Pass.Textures = {{"SourceDepth", Mip == 0 ? InRequest.Depth.Texture : InProduct.Texture, Mip == 0 ? 0 : Mip - 1,
		                  1, EResourceState::ShaderRead, false, Mip == 0 ? InRequest.Depth.bInitialized : false},
		                 {"OutputDepth", InProduct.Texture, Mip, 1, EResourceState::ShaderWrite, true, false}};
		Pass.Parameters = {{"DepthParameters.Width", FMaterialValue::Uint(Size.Width)},
		                   {"DepthParameters.Height", FMaterialValue::Uint(Size.Height)}};
		if (Mip == 0)
		{
			Pass.Parameters.push_back(
			    {"DepthParameters.Viewport", FMaterialValue::Float(FVec4{View.X, View.Y, View.Width, View.Height})});
			Pass.Parameters.push_back(
			    {"DepthParameters.DepthRange",
			     FMaterialValue::Float(FVec2{View.MinDepth, 1.f / (View.MaxDepth - View.MinDepth)})});
			Pass.Parameters.push_back({"DepthParameters.FarDepth",
			                           FMaterialValue::Float(GetDepthClearValue(InRequest.View.DepthConvention))});
		}
		else
		{
			const auto Previous = InProduct.MipSizes[Mip - 1];
			Pass.Parameters.push_back({"DepthParameters.SourceWidth", FMaterialValue::Uint(Previous.Width)});
			Pass.Parameters.push_back({"DepthParameters.SourceHeight", FMaterialValue::Uint(Previous.Height)});
			const bool bMaximum = (InRequest.Reduction == EDepthReduction::Nearest) ==
			                      (InRequest.View.DepthConvention == EDepthConvention::Reversed);
			Pass.Parameters.push_back({"DepthParameters.bMaximum", FMaterialValue::Uint(bMaximum ? 1 : 0)});
		}
		AddComputePass(InSession, InGraph, std::move(Pass), bInDeferPreparation);
	}
}
} // namespace

void FHierarchicalDepthProduct::Validate(const FRenderGraph& InGraph, const FRenderView& InView) const
{
	const auto ExpectedViewport = InView.Viewport.value_or(FViewport{0, 0, float(InView.Width), float(InView.Height)});
	const auto* Storage = Texture ? Texture->GetStorage() : nullptr;
	if (!Storage || !Lifetime || !SourceIdentity || GraphIdentity != InGraph.GetIdentity() ||
	    ViewIdentity != InView.Identity || ViewRevision != InView.Revision ||
	    ViewProjection.Values != InView.ViewProjection.Values || Viewport != ExpectedViewport ||
	    Eye.X != InView.Eye.X || Eye.Y != InView.Eye.Y || Eye.Z != InView.Eye.Z || Storage->Width != InView.Width ||
	    Storage->Height != InView.Height || Convention != InView.DepthConvention)
	{
		throw std::invalid_argument("HZB product does not belong to the current graph and source view");
	}
}

void FHierarchicalDepthProducer::BeginFrame(const FRenderGraph& InGraph)
{
	if (Graph != InGraph.GetIdentity())
	{
		Graph = InGraph.GetIdentity();
		Stats = {};
	}
}

FHierarchicalDepthProduct FHierarchicalDepthProducer::Request(FRenderSession& InSession, FRenderGraph& InGraph,
                                                              const FHierarchicalDepthRequest& InRequest,
                                                              bool bInDeferPreparation)
{
	BeginFrame(InGraph);
	const auto* Depth = InRequest.Depth.Texture ? InRequest.Depth.Texture->GetDepthTarget() : nullptr;
	if (!Depth || InRequest.Depth.Kind != ERenderTargetKind::Texture || !InRequest.Depth.Lifetime ||
	    InRequest.View.DepthConvention > EDepthConvention::Reversed ||
	    InRequest.Reduction > EDepthReduction::Farthest || Depth->Width != InRequest.View.Width ||
	    Depth->Height != InRequest.View.Height || !InRequest.View.Identity)
	{
		throw std::invalid_argument("HZB requires a matching view and live sampled depth source");
	}
	const auto View = InRequest.View.Viewport.value_or(FViewport{0, 0, float(Depth->Width), float(Depth->Height)});
	ValidateViewport(View, {Depth->Width, Depth->Height});
	if (!(View.MaxDepth > View.MinDepth) || !std::isfinite(1.f / (View.MaxDepth - View.MinDepth)))
	{
		throw std::invalid_argument("HZB requires an invertible viewport depth range");
	}
	std::array<float, 25> Camera;
	std::copy(InRequest.View.ViewProjection.Values.begin(), InRequest.View.ViewProjection.Values.end(), Camera.begin());
	const std::array Region{View.X,
	                        View.Y,
	                        View.Width,
	                        View.Height,
	                        View.MinDepth,
	                        View.MaxDepth,
	                        InRequest.View.Eye.X,
	                        InRequest.View.Eye.Y,
	                        InRequest.View.Eye.Z};
	std::copy(Region.begin(), Region.end(), Camera.begin() + 16);
	if (!std::all_of(Camera.begin(), Camera.end(),
	                 [](float InValue)
	                 {
		                 return std::isfinite(InValue);
	                 }))
	{
		throw std::invalid_argument("HZB requires a finite source camera");
	}
	auto Found = std::find_if(Entries.begin(), Entries.end(),
	                          [&](const auto& InEntry)
	                          {
		                          return InEntry.Source == InRequest.Depth.Texture &&
		                                 InEntry.View == InRequest.View.Identity &&
		                                 InEntry.Product.Convention == InRequest.View.DepthConvention &&
		                                 InEntry.Product.Reduction == InRequest.Reduction;
	                          });
	if (Found == Entries.end())
	{
		Entries.push_back({InRequest.Depth.Texture, InRequest.View.Identity, CreateProduct(InSession, InRequest)});
		Found = std::prev(Entries.end());
	}
	if (Found->Product.GraphIdentity == Graph)
	{
		Found->Product.Validate(InGraph, InRequest.View);
	}
	else
	{
		Generate(InSession, InGraph, InRequest, Found->Product, bInDeferPreparation);
		Found->Product.GraphIdentity = Graph;
		Found->Product.SourceIdentity = InRequest.Depth.Texture->GetIdentity();
		Found->Product.ViewIdentity = InRequest.View.Identity;
		Found->Product.ViewRevision = InRequest.View.Revision;
		Found->Product.ViewProjection = InRequest.View.ViewProjection;
		Found->Product.Eye = InRequest.View.Eye;
		Found->Product.Viewport = View;
		++Stats.Products;
		Stats.Dispatches += static_cast<std::uint32_t>(Found->Product.MipSizes.size());
		Stats.Bytes += Found->Product.Bytes;
	}
	++Stats.Consumers;
	return Found->Product;
}

void FHierarchicalDepthProducer::EndFrame()
{
	std::erase_if(Entries,
	              [&](const auto& InEntry)
	              {
		              return InEntry.Product.GraphIdentity != Graph;
	              });
}
} // namespace Hyperion
