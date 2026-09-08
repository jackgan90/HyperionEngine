#include "Hyperion/DebugUI/DebugUIPlugin.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <iomanip>
#include <sstream>

namespace Hyperion
{
namespace
{
std::string Fixed(double InValue, int InDecimals = 2)
{
	std::ostringstream Out;
	Out << std::fixed << std::setprecision(InDecimals) << InValue;
	return Out.str();
}
} // namespace

FDebugActions DrawFrameCaptureControls(FGui& InGui, FAppSettings& InSettings, const FFrameCaptureMetrics& InMetrics)
{
	FDebugActions Actions;
	InGui.Text("RENDERDOC");
	if (InMetrics.bCompiled)
	{
		bool bEnabled =
		    std::find(InSettings.Plugins.begin(), InSettings.Plugins.end(), "renderdoc") != InSettings.Plugins.end();
		if (InGui.Checkbox("RenderDoc plugin (restart)", bEnabled))
		{
			if (bEnabled)
			{
				InSettings.Plugins.push_back("renderdoc");
			}
			else
			{
				std::erase(InSettings.Plugins, std::string("renderdoc"));
			}
		}
		Actions.bCaptureRdc = InGui.Button("Capture RDC", InMetrics.bAvailable && !InMetrics.bBusy);
		Actions.CaptureRdcBounds = InGui.LastItemBounds();
		Actions.bOpenRdc = InGui.Button("Open last capture",
		                                InMetrics.bAvailable && !InMetrics.bBusy && !InMetrics.LastCapture.empty());
		Actions.OpenRdcBounds = InGui.LastItemBounds();
		InGui.Checkbox("Open automatically after capture", InSettings.bRenderDocAutoOpen);
		Actions.AutoOpenRdcBounds = InGui.LastItemBounds();
	}
	InGui.TextWrapped(InMetrics.Status);
	if (!InMetrics.LastCapture.empty())
	{
		InGui.TextWrapped(InMetrics.LastCapture);
	}
	if (!InMetrics.OpenStatus.empty())
	{
		InGui.TextWrapped(InMetrics.OpenStatus);
	}
	return Actions;
}

FDebugActions DrawDebugPanel(FGui& InGui, FAppSettings& InSettings, const FDebugMetrics& InMetrics, FSize InLogicalSize)
{
	FDebugActions Actions;
	if (InGui.BeginPanel("Hyperion diagnostics", {24, 24}, {326, std::max(150.f, float(InLogicalSize.Height) - 48)}))
	{
		InGui.Text("H Y P E R I O N");
		InGui.Text("Rendering lab  /  " + InSettings.RHIBackend);
		if (!InMetrics.AssetStatus.empty())
		{
			InGui.Separator();
			InGui.Text(InMetrics.bSceneViewer ? "SCENE" : "MODEL");
			InGui.TextWrapped(InMetrics.AssetStatus);
			InGui.Text(InMetrics.bSceneViewer ? "Arrows: move | Drag: orbit | Wheel: dolly"
			                                  : "Drag: orbit | Wheel: zoom");
			InGui.Text("Home: fit | Tab: toggle panel");
		}
		InGui.Separator();
		InGui.Text(InMetrics.Device.Adapter);
		InGui.Text(std::string("Validation: ") + (InMetrics.Device.bDebugLayer ? "enabled" : "unavailable") +
		           "  |  Errors: " + std::to_string(InMetrics.Device.ValidationErrors));
		const float Ms = InMetrics.FrameMilliseconds.empty() ? 0 : InMetrics.FrameMilliseconds.back();
		InGui.Text(Fixed(Ms) + " ms  /  " + Fixed(Ms > 0 ? 1000 / Ms : 0, 0) + " FPS");
		InGui.Plot("Frame interval", InMetrics.FrameMilliseconds, std::max(34.f, Ms * 1.2f));
		InGui.Separator();
		InGui.Text("EXPERIMENT");
		constexpr std::array<std::string_view, 5> Ids{"triangle_scale", "clear_red", "clear_green", "clear_blue",
		                                              "vsync"};
		InGui.EditProperties(SettingsType(), &InSettings,
		                     InSettings.ModelSource.empty() ? std::span(Ids) : std::span(Ids).subspan(1));
		bool bTriangle =
		    std::find(InSettings.Plugins.begin(), InSettings.Plugins.end(), "triangle") != InSettings.Plugins.end();
		if (InSettings.ModelSource.empty() && InGui.Checkbox("Triangle plugin (restart)", bTriangle))
		{
			if (bTriangle)
			{
				InSettings.Plugins.insert(InSettings.Plugins.begin(), "triangle");
			}
			else
			{
				std::erase(InSettings.Plugins, std::string("triangle"));
			}
		}
		Actions.bSave = InGui.Button("Save experiment");
		Actions.bCapture = InGui.Button("Capture screenshot");
		InGui.Separator();
		const auto CaptureActions = DrawFrameCaptureControls(InGui, InSettings, InMetrics.FrameCapture);
		Actions.bCaptureRdc = CaptureActions.bCaptureRdc;
		Actions.bOpenRdc = CaptureActions.bOpenRdc;
		Actions.CaptureRdcBounds = CaptureActions.CaptureRdcBounds;
		Actions.OpenRdcBounds = CaptureActions.OpenRdcBounds;
		Actions.AutoOpenRdcBounds = CaptureActions.AutoOpenRdcBounds;
		InGui.Separator();
		InGui.Text("EXECUTION / MEMORY");
		for (const auto& Thread : InMetrics.Threads)
		{
			InGui.Text(Thread.Name == "Main" ? "Main  window / input"
			                                 : Thread.Name + "  " + std::to_string(Thread.Executed) + " tasks");
		}
		std::size_t Tracked{};
		for (int I = 0; I < static_cast<int>(EMemoryTag::Count); ++I)
		{
			Tracked += MemoryStats(static_cast<EMemoryTag>(I)).LiveBytes;
		}
		InGui.Text("CPU hooked: " + Fixed(Tracked / 1048576.0) + " MiB");
		InGui.Text("GPU allocated: " + Fixed(InMetrics.Device.GpuAllocationBytes / 1048576.0) + " MiB");
		InGui.Text("Frame interval includes present waits.");
	}
	InGui.EndPanel();
	return Actions;
}

struct FDebugUiPlugin::FImpl
{
	IRHIDevice& Device;
	FShaderCompiler& Compiler;
	FTaskSystem& Tasks;
	FImage Font;
	FPipeline Pipeline;
	FTexture Texture;
	std::vector<FDrawPacket> Draws;
	FResourceBindingSet Bindings;
};

FDebugUiPlugin::FDebugUiPlugin(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FImage InFont)
    : Impl(std::make_unique<FImpl>(FImpl{InDevice, InCompiler, InTasks, std::move(InFont), {}, {}, {}}))
{
}

FDebugUiPlugin::~FDebugUiPlugin() = default;

void FDebugUiPlugin::Start()
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Main});
	FPipelineDesc Desc;
	Desc.State.bBlend = true;
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
		                                                              P.Device.GetCapabilities().ShaderFormat);
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
		    P.Pipeline = P.Device.CreatePipeline(Desc);
		    P.Texture = P.Device.CreateTexture(P.Font);
		    FSamplerDesc Sampler;
		    Sampler.U = Sampler.V = Sampler.W = ERHIAddressMode::Clamp;
		    const auto FontSampler = P.Device.CreateSampler(Sampler);
		    P.Bindings = P.Device.CreateBindingSet({Desc.Layout, {{1, {P.Texture}}, {2, {FontSampler}}}});
		    P.Font = {};
	    }));
}

void FDebugUiPlugin::Stop() noexcept
{
	Impl->Tasks.Require({EDomain::Main});
	Impl->Tasks.Wait(Impl->Tasks.Dispatch({EDomain::Rhi, 0},
	                                      [this]
	                                      {
		                                      Impl->Draws.clear();
		                                      Impl->Pipeline = {};
		                                      Impl->Texture = {};
		                                      Impl->Bindings = {};
	                                      }));
}

void FDebugUiPlugin::Prepare(const FGuiDrawData& InData)
{
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Rhi, 0});
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
		Draw.Bindings = P.Bindings;
		Draw.VertexStride = sizeof(FGuiVertex);
		Draw.IndexCount = Command.IndexCount;
		Draw.FirstIndex = Command.FirstIndex;
		Draw.VertexOffset = Command.VertexOffset;
		Draw.ConstantBindings = {{0, Slice}};
		Draw.Scissor = Scissor;
		P.Draws.push_back(std::move(Draw));
	}
}

void FDebugUiPlugin::Build(FRenderGraph& InGraph, const FRenderFrame&)
{
	Impl->Tasks.Require({EDomain::Render});
	if (Impl->Draws.empty())
	{
		return;
	}
	FColorPass Pass;
	Pass.Commands.Name = "Debug UI";
	Pass.Commands.Draws = Impl->Draws;
	InGraph.Add(std::move(Pass));
}

void RegisterDebugUiPlugin(FPluginRegistry& InRegistry, IRHIDevice& InDevice, FShaderCompiler& InCompiler,
                           FTaskSystem& InTasks, const FImage& InFont)
{
	InRegistry.Add({"debug-ui",
	                {},
	                [&]
	                {
		                return std::make_unique<FDebugUiPlugin>(InDevice, InCompiler, InTasks, InFont);
	                }});
}
} // namespace Hyperion
