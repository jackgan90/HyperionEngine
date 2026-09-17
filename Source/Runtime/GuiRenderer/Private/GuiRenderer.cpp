#include "Hyperion/GuiRenderer/GuiRenderer.h"
#include "Hyperion/Core/Profiling.h"
#include <algorithm>
#include <cstddef>
#include <map>

namespace Hyperion
{
struct FGuiRenderer::FImpl
{
	struct FGeometry
	{
		std::vector<std::byte> Bytes;
		FBuffer Buffer;
	};

	struct FImageBinding
	{
		FTexture Texture;
		FResourceBindingSet Bindings;
		FRenderTargetSource Source;
	};

	FGeometry Vertices;
	FGeometry Indices;
	FMat4 Projection{};
	FBufferSlice ProjectionSlice;
	std::map<std::uint64_t, FImageBinding> Images;
	FBuffer Geometry(FGeometry& InCache, std::span<const std::byte> InBytes, ERHIBufferUsage InUsage);
	FBufferSlice Constants(const FGuiDrawData& InData);
	std::map<std::uint64_t, FResourceBindingSet> ResolveImages(const std::vector<FGuiTextureBinding>& InTextures);

	FImpl(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FImage InFont)
	    : Device(InDevice), Compiler(InCompiler), Tasks(InTasks), Font(std::move(InFont))
	{
	}

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
    : Impl(std::make_shared<FImpl>(InDevice, InCompiler, InTasks, std::move(InFont)))
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
		                                      Impl->Vertices = {};
		                                      Impl->Indices = {};
		                                      Impl->ProjectionSlice = {};
		                                      Impl->Images.clear();
		                                      Impl->Pipeline = {};
		                                      Impl->Texture = {};
		                                      Impl->Bindings = {};
		                                      Impl->Layout = {};
		                                      Impl->Sampler = {};
	                                      }));
}

FBuffer FGuiRenderer::FImpl::Geometry(FGeometry& InCache, std::span<const std::byte> InBytes, ERHIBufferUsage InUsage)
{
	if (!std::equal(InBytes.begin(), InBytes.end(), InCache.Bytes.begin(), InCache.Bytes.end()))
	{
		std::vector<std::byte> Bytes(InBytes.begin(), InBytes.end());
		auto Buffer = Device.CreateBuffer({InBytes.size(), BufferUsage(InUsage)}, InBytes);
		InCache.Bytes = std::move(Bytes);
		InCache.Buffer = std::move(Buffer);
	}
	return InCache.Buffer;
}

FBufferSlice FGuiRenderer::FImpl::Constants(const FGuiDrawData& InData)
{
	auto Matrix = Identity();
	const float W = InData.DisplaySize.X;
	const float H = InData.DisplaySize.Y;
	Matrix.Values[0] = 2 / W;
	Matrix.Values[5] = -2 / H;
	Matrix.Values[12] = -1 - 2 * InData.DisplayPosition.X / W;
	Matrix.Values[13] = 1 + 2 * InData.DisplayPosition.Y / H;
	if (!ProjectionSlice.Buffer || Matrix.Values != Projection.Values)
	{
		const auto Buffer = Device.CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
		auto Slice = Device.PublishConstantSlice(Buffer, 0, std::as_bytes(std::span(&Matrix, 1)));
		Projection = Matrix;
		ProjectionSlice = std::move(Slice);
	}
	return ProjectionSlice;
}

std::map<std::uint64_t, FResourceBindingSet> FGuiRenderer::FImpl::ResolveImages(
    const std::vector<FGuiTextureBinding>& InTextures)
{
	std::map<std::uint64_t, FResourceBindingSet> Result{{1, Bindings}};
	std::map<std::uint64_t, FImageBinding> Next;
	for (const auto& Binding : InTextures)
	{
		if (Binding.Id <= 1 || Result.contains(Binding.Id) || !Preparation)
		{
			throw std::invalid_argument("Invalid or duplicate GUI texture binding");
		}
		// Resolve every frame to validate the current owner and retain its resource lifetime.
		const auto Image = Preparation->ResolveTexture(Binding.Source.Texture, Binding.Source.Lifetime);
		const auto Cached = Images.find(Binding.Id);
		auto Set = Cached != Images.end() && Cached->second.Texture == Image
		               ? Cached->second.Bindings
		               : Device.CreateBindingSet({Layout, {{1, {Image}}, {2, {Sampler}}}});
		Next.emplace(Binding.Id, FImageBinding{Image, Set, Binding.Source});
		Result.emplace(Binding.Id, std::move(Set));
	}
	Images.swap(Next);
	return Result;
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
		P.Vertices = {};
		P.Indices = {};
		P.Images.clear();
		return;
	}
	const auto VertexBuffer = Geometry(P.Vertices, std::as_bytes(std::span(InData.Vertices)), ERHIBufferUsage::Vertex);
	const auto IndexBuffer = Geometry(P.Indices, std::as_bytes(std::span(InData.Indices)), ERHIBufferUsage::Index);
	const auto Slice = Constants(InData);
	const auto TextureBindings = ResolveImages(InTextures);
	const float W = InData.DisplaySize.X;
	const float H = InData.DisplaySize.Y;
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
		Draw.Vertices = VertexBuffer;
		Draw.Indices = IndexBuffer;
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
		if (bInClear)
		{
			Pass.Color->Actions.Load = EAttachmentLoad::Clear;
			Pass.Color->Clear = {.01f, .01f, .01f, 1};
		}
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
