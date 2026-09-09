#include "DepthPreview.h"
#include "Hyperion/Renderer/RenderSession.h"
#include "RenderResourcesInternal.h"

namespace Hyperion
{
namespace
{
FDepthPreview CreatePreview(IRHIDevice& InDevice, FShaderCompiler& InCompiler)
{
	FDepthPreview Result;
	Result.Layout = InDevice.CreateBindingLayout({{{ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel}}});
	const std::array<FVec2, 3> Positions{{{-1, -1}, {3, -1}, {-1, 3}}};
	const std::array<std::uint32_t, 3> Indices{0, 1, 2};
	Result.Draw.Vertices = InDevice.CreateBuffer(std::as_bytes(std::span(Positions)));
	Result.Draw.Indices = InDevice.CreateBuffer(std::as_bytes(std::span(Indices)));
	Result.Draw.VertexStride = sizeof(FVec2);
	Result.Draw.IndexCount = 3;
	FPipelineDesc Pipeline;
	Pipeline.Layout = Result.Layout;
	Pipeline.VertexStride = sizeof(FVec2);
	Pipeline.Attributes = {{"POSITION", 0, EVertexFormat::Float2, 0}};
	Pipeline.Vertex = InCompiler.Compile("DepthPreview.hlsl", "VSMain", EShaderStage::Vertex,
	                                     InDevice.GetCapabilities().ShaderFormat);
	Pipeline.Pixel =
	    InCompiler.Compile("DepthPreview.hlsl", "PSMain", EShaderStage::Pixel, InDevice.GetCapabilities().ShaderFormat);
	Result.Draw.Pipeline = InDevice.CreatePipeline(Pipeline);
	return Result;
}
} // namespace

void FDepthPreview::Collect()
{
	if (Lifetime.expired())
	{
		Draw.Bindings = {};
		Texture = {};
	}
}

FGraphicsPass FRenderResourceService::BuildDepthPreview(std::shared_ptr<const FMaterialTextureSource> InSource,
                                                        std::shared_ptr<const void> InLifetime, FViewport InViewport)
{
	return GetPreparation().BuildDepthPreview(std::move(InSource), std::move(InLifetime), InViewport);
}

FGraphicsPass FRenderResourcePreparation::BuildDepthPreview(std::shared_ptr<const FMaterialTextureSource> InSource,
                                                            std::shared_ptr<const void> InLifetime,
                                                            FViewport InViewport) const
{
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::lock_guard Lock(Owner.Mutex);
	if (Owner.bClosed || !InSource || !InSource->GetDepthTarget() || !InLifetime)
	{
		throw std::invalid_argument("Depth preview requires a live depth target");
	}
	Owner.EnsureMaterialCaches();
	Owner.TrackScope(InLifetime);
	auto& Preview = Owner.DepthPreview;
	if (!Preview.Draw.Pipeline)
	{
		Preview = CreatePreview(Owner.Device, Owner.Compiler);
	}
	const auto Texture = Owner.MaterialGpu->GetTexture(InSource, {InLifetime});
	Preview.Lifetime = InLifetime;
	if (Preview.Texture != Texture)
	{
		Preview.Draw.Bindings = Owner.Device.CreateBindingSet({Preview.Layout, {{0, {Texture}}}});
		Preview.Texture = Texture;
	}
	FGraphicsPass Pass;
	Pass.Commands.Name = "Shadow depth preview";
	Pass.Commands.Viewport = InViewport;
	Pass.Commands.SampledDepth = {Texture};
	auto Draw = Preview.Draw;
	Draw.Scissor = {static_cast<int>(InViewport.X), static_cast<int>(InViewport.Y),
	                static_cast<int>(InViewport.X + InViewport.Width),
	                static_cast<int>(InViewport.Y + InViewport.Height)};
	Pass.Commands.Draws.push_back(std::move(Draw));
	return Pass;
}

void FRenderSession::AppendDepthPreview(FRenderGraph& InGraph, std::shared_ptr<const FMaterialTextureSource> InSource,
                                        std::shared_ptr<const void> InLifetime, FViewport InViewport, bool bInDeferred)
{
	Tasks.Require({EDomain::Render});
	if (bInDeferred)
	{
		InGraph.AddDeferred(
		    [Preparation = Resources.GetPreparation(), Source = std::move(InSource), Lifetime = std::move(InLifetime),
		     InViewport]
		    {
			    std::vector<FColorPass> Passes;
			    Passes.push_back(Preparation.BuildDepthPreview(Source, Lifetime, InViewport));
			    return Passes;
		    });
		return;
	}
	auto Result =
	    DispatchAsync<FGraphicsPass>(Tasks, {EDomain::Rhi, 0},
	                                 [this, Source = std::move(InSource), Lifetime = std::move(InLifetime), InViewport]
	                                 {
		                                 return Resources.BuildDepthPreview(Source, Lifetime, InViewport);
	                                 });
	Tasks.Wait(Result.Task());
	InGraph.Add(*Result.GetReady());
}
} // namespace Hyperion
