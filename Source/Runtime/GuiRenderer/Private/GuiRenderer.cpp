#include "Hyperion/GuiRenderer/GuiRenderer.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <cstddef>
#include <map>

namespace Hyperion
{
struct FGuiRenderer::FImpl
{
	IRHIDevice& Device;
	FShaderCompiler& Compiler;
	FTaskSystem& Tasks;
	FImage Font;
	FPipeline Pipeline;
	FTexture Texture;
	std::vector<FDrawPacket> Draws;
	FResourceBindingSet Bindings;
	FResourceBindingLayout Layout;
	FSampler Sampler;
	std::optional<FRenderResourcePreparation> Preparation;
	void Prepare(const FGuiDrawData& InData, const std::vector<FGuiTextureBinding>& InTextures = {});
};

FGuiRenderer::FGuiRenderer(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FImage InFont,
                           FRenderResourceService* InResources)
    : Impl(std::make_shared<FImpl>(FImpl{InDevice, InCompiler, InTasks, std::move(InFont), {}, {}, {}}))
{
	if (InResources)
	{
		Impl->Preparation = InResources->GetPreparation();
	}
}

FGuiRenderer::~FGuiRenderer() = default;

void FGuiRenderer::Start()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	FPipelineDesc Desc;
	Desc.State.bBlend = true;
	Desc.Target.bSrgb = true;
	Desc.State.SourceRgb = ERHIBlendFactor::SourceAlpha;
	Desc.State.DestinationRgb = ERHIBlendFactor::InverseSourceAlpha;
	Desc.State.DestinationAlpha = ERHIBlendFactor::InverseSourceAlpha;
	Desc.VertexStride = sizeof(FGuiVertex);
	P.Tasks.Wait(P.Tasks.Dispatch({EDomain::Worker},
	                              [&]
	                              {
		                              Desc.Vertex = P.Compiler.Compile("Gui.hlsl", "VSMain", EShaderStage::Vertex,
		                                                               P.Device.GetCapabilities().ShaderFormat);
		                              Desc.Pixel = P.Compiler.Compile("Gui.hlsl", "PSMain", EShaderStage::Pixel,
		                                                              P.Device.GetCapabilities().ShaderFormat,
		                                                              {{{"HYP_GUI_SRGB", "1"}}});
	                              }));
	Desc.Attributes = {{"POSITION", 0, EVertexFormat::Float2, offsetof(FGuiVertex, Position)},
	                   {"TEXCOORD", 0, EVertexFormat::Float2, offsetof(FGuiVertex, Uv)},
	                   {"COLOR", 0, EVertexFormat::Unorm8x4, offsetof(FGuiVertex, Color)}};
	P.Tasks.Wait(P.Tasks.Dispatch(
	    {EDomain::Rhi, 0},
	    [&P, Desc = std::move(Desc)]() mutable
	    {
		    FResourceBindingLayoutDesc Layout;
		    Layout.Slots = {{ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Vertex, 0, 0, 1, 64},
		                    {ERHIBindingKind::Texture2D, ERHIShaderVisibility::Pixel, 0},
		                    {ERHIBindingKind::Sampler, ERHIShaderVisibility::Pixel, 0}};
		    Desc.Layout = P.Device.CreateBindingLayout(Layout);
		    P.Layout = Desc.Layout;
		    P.Pipeline = P.Device.CreatePipeline(Desc);
		    P.Texture = P.Device.CreateTexture(P.Font);
		    FSamplerDesc Sampler;
		    Sampler.U = Sampler.V = Sampler.W = ERHIAddressMode::Clamp;
		    const auto FontSampler = P.Device.CreateSampler(Sampler);
		    P.Sampler = FontSampler;
		    P.Bindings = P.Device.CreateBindingSet({Desc.Layout, {{1, {P.Texture}}, {2, {FontSampler}}}});
		    P.Font = {};
	    }));
}

void FGuiRenderer::Stop() noexcept
{
	Impl->Tasks.Require({EDomain::Main});
	Impl->Tasks.Wait(Impl->Tasks.Dispatch({EDomain::Rhi, 0},
	                                      [this]
	                                      {
		                                      Impl->Draws.clear();
		                                      Impl->Pipeline = {};
		                                      Impl->Texture = {};
		                                      Impl->Bindings = {};
		                                      Impl->Layout = {};
		                                      Impl->Sampler = {};
	                                      }));
}

void FGuiRenderer::Prepare(const FGuiDrawData& InData)
{
	Impl->Prepare(InData);
}

void FGuiRenderer::FImpl::Prepare(const FGuiDrawData& InData, const std::vector<FGuiTextureBinding>& InTextures)
{
	HYP_PERF_SCOPE_C(Rhi, PrepareGuiResources);
	auto& P = *this;
	P.Tasks.Require({EDomain::Rhi, 0});
	if (!P.Pipeline)
	{
		throw std::logic_error("GUI preparation requires a started renderer");
	}
	P.Draws.clear();
	if (InData.Vertices.empty() || InData.Indices.empty())
	{
		return;
	}
	const auto VertexBytes = std::as_bytes(std::span(InData.Vertices));
	const auto IndexBytes = std::as_bytes(std::span(InData.Indices));
	auto Vertices = P.Device.CreateBuffer({VertexBytes.size(), BufferUsage(ERHIBufferUsage::Vertex)}, VertexBytes);
	auto Indices = P.Device.CreateBuffer({IndexBytes.size(), BufferUsage(ERHIBufferUsage::Index)}, IndexBytes);
	auto Matrix = Identity();
	const float W = InData.DisplaySize.X;
	const float H = InData.DisplaySize.Y;
	Matrix.Values[0] = 2 / W;
	Matrix.Values[5] = -2 / H;
	Matrix.Values[12] = -1 - 2 * InData.DisplayPosition.X / W;
	Matrix.Values[13] = 1 + 2 * InData.DisplayPosition.Y / H;
	const auto Constants = P.Device.CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
	const auto Slice = P.Device.PublishConstantSlice(Constants, 0, std::as_bytes(std::span(&Matrix, 1)));
	std::map<std::uint64_t, FResourceBindingSet> TextureBindings{{1, P.Bindings}};
	for (const auto& Binding : InTextures)
	{
		if (Binding.Id <= 1 || TextureBindings.contains(Binding.Id) || !P.Preparation)
		{
			throw std::invalid_argument("Invalid or duplicate GUI texture binding");
		}
		const auto ImageTexture = P.Preparation->ResolveTexture(Binding.Source.Texture, Binding.Source.Lifetime);
		TextureBindings.emplace(Binding.Id,
		                        P.Device.CreateBindingSet({P.Layout, {{1, {ImageTexture}}, {2, {P.Sampler}}}}));
	}
	for (const auto& Command : InData.Commands)
	{
		FRect Scissor{static_cast<std::int32_t>(
		                  std::max(0.f, (Command.Clip.X - InData.DisplayPosition.X) * InData.FramebufferScale.X)),
		              static_cast<std::int32_t>(
		                  std::max(0.f, (Command.Clip.Y - InData.DisplayPosition.Y) * InData.FramebufferScale.Y)),
		              static_cast<std::int32_t>(
		                  std::min(W * InData.FramebufferScale.X,
		                           (Command.Clip.Z - InData.DisplayPosition.X) * InData.FramebufferScale.X)),
		              static_cast<std::int32_t>(
		                  std::min(H * InData.FramebufferScale.Y,
		                           (Command.Clip.W - InData.DisplayPosition.Y) * InData.FramebufferScale.Y))};
		if (Scissor.Right <= Scissor.Left || Scissor.Bottom <= Scissor.Top || !Command.IndexCount)
		{
			continue;
		}
		FDrawPacket Draw;
		Draw.Pipeline = P.Pipeline;
		Draw.Vertices = Vertices;
		Draw.Indices = Indices;
		const auto Found = TextureBindings.find(Command.TextureId);
		if (Found == TextureBindings.end())
		{
			throw std::invalid_argument("GUI draw references an unregistered texture");
		}
		Draw.Bindings = Found->second;
		Draw.VertexStride = sizeof(FGuiVertex);
		Draw.IndexCount = Command.IndexCount;
		Draw.FirstIndex = Command.FirstIndex;
		Draw.VertexOffset = Command.VertexOffset;
		Draw.ConstantBindings = {{0, Slice}};
		Draw.Scissor = Scissor;
		P.Draws.push_back(std::move(Draw));
	}
}

void FGuiRenderer::Build(FRenderGraph& InGraph)
{
	Impl->Tasks.Require({EDomain::Render});
	if (Impl->Draws.empty())
	{
		return;
	}
	FGraphicsPass Pass;
	Pass.Name = "Debug UI";
	Pass.Color = FGraphColorAttachment{InGraph.ImportBackbuffer(), {}, {}, EGraphColorView::Srgb};
	Pass.Batches.push_back({{Impl->Draws}, true});
	InGraph.Add(std::move(Pass));
}

void FGuiRenderer::BuildDeferred(FRenderGraph& InGraph, FGuiDrawData InData, std::vector<FGuiTextureBinding> InTextures,
                                 bool bInClear)
{
	Impl->Tasks.Require({EDomain::Render});
	if (InData.Commands.empty() && !bInClear)
	{
		return;
	}
	FGraphicsPass Pass;
	if (Impl->Preparation)
	{
		FRenderSceneSnapshot Snapshot;
		Snapshot.Targets =
		    FRenderPassTargets::ColorOnly(bInClear ? std::optional(FVec4{.01f, .01f, .01f, 1}) : std::nullopt);
		Snapshot.Targets.Name = "GUI";
		Snapshot.Targets.Color->View = EGraphColorView::Srgb;
		for (const auto& Binding : InTextures)
		{
			if (Binding.Source.Texture && Binding.Source.Texture->IsGpuGenerated())
			{
				Snapshot.Targets.Reads.push_back(Binding.Source);
			}
		}
		Pass = Impl->Preparation->DeclarePass(InGraph, Snapshot);
	}
	else
	{
		Pass.Name = "Debug UI";
		Pass.Color = FGraphColorAttachment{InGraph.ImportBackbuffer(), {}, {}, EGraphColorView::Srgb};
	}
	Pass.Prepare = [Owner = std::weak_ptr<FImpl>(Impl), Data = std::move(InData), Textures = std::move(InTextures)]
	{
		const auto State = Owner.lock();
		if (!State)
		{
			throw std::logic_error("GUI preparation owner has been destroyed");
		}
		State->Prepare(Data, Textures);
		std::vector<FGraphicsDrawBatch> Batches;
		Batches.push_back({{std::move(State->Draws)}, true});
		return Batches;
	};
	InGraph.Add(std::move(Pass));
}
} // namespace Hyperion
