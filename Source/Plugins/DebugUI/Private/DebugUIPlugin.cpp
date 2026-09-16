#include "Hyperion/DebugUI/DebugUIPlugin.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/GuiRenderer/GuiRenderer.h"
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

void DrawPipelineControls(FGui& InGui, FAppSettings& InSettings, const FDebugMetrics& InMetrics)
{
	InGui.Text("RENDER PIPELINE");
	bool bDeferred = InSettings.RenderPipeline == "deferred";
	if (InGui.Checkbox("Deferred shading", bDeferred))
	{
		InSettings.RenderPipeline = bDeferred ? "deferred" : "forward";
	}
	bool bHighPrecision = InSettings.GBufferLayout == "high";
	if (InGui.Checkbox("High precision GBuffer", bHighPrecision))
	{
		InSettings.GBufferLayout = bHighPrecision ? "high" : "compact";
	}
	constexpr std::array<std::string_view, 4> PipelineIds{"clustered_lighting", "reversed_z", "exposure",
	                                                      "gbuffer_debug"};
	InGui.EditProperties(SettingsType(), &InSettings, PipelineIds);
	InGui.Text(InMetrics.bActiveReversedZ ? "Active depth: Reversed Z" : "Active depth: Standard Z");
	if (InSettings.bReversedZ != InMetrics.bActiveReversedZ)
	{
		InGui.TextWrapped("Depth change pending: save experiment and restart to apply.");
	}
	InGui.TextWrapped("GBuffer: 0 lit, 1 base, 2 normal, 3 rough/metal/AO, 4 emissive, 5 depth, 6 surface normal");
	constexpr std::array<std::string_view, 6> ContactIds{"contact_shadow_length", "contact_shadow_thickness",
	                                                     "contact_shadow_bias",   "contact_shadow_steps",
	                                                     "contact_shadow_debug",  "hierarchical_depth_mip"};
	InGui.EditProperties(SettingsType(), &InSettings, ContactIds);
	InGui.TextWrapped("Contact debug: 0 lit, 1 visibility mask, 2 HZB mip (requests depth even with contact off)");
	InGui.Text(InMetrics.bContactShadows ? "Contact shadows: active" : "Contact shadows: inactive");
	InGui.Text("HZB: consumers=" + std::to_string(InMetrics.HierarchicalDepthConsumers) +
	           " dispatches=" + std::to_string(InMetrics.HierarchicalDepthDispatches) +
	           " memory=" + Fixed(InMetrics.HierarchicalDepthBytes / 1048576.0) + " MiB");
	if (!bDeferred)
	{
		InGui.TextWrapped("Contact shadows require Deferred GBuffer depth; Forward has no depth prepass.");
	}
	InGui.Separator();
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
		InGui.Checkbox("Contact shadows", InSettings.bContactShadows);
		Actions.ContactShadowBounds = InGui.LastItemBounds();
		const auto Progress = InMetrics.FramePipeline;
		InGui.Text("CPU sent " + std::to_string(Progress.Submitted) + " | R done " +
		           std::to_string(Progress.RenderCompleted) + " | RHI " + std::to_string(Progress.RhiCompleted));
		InGui.Text("Result frame " + std::to_string(InMetrics.ResultFrame) + " | lead limits " +
		           std::to_string(InMetrics.FrameLimits.MainLead) + " / " +
		           std::to_string(InMetrics.FrameLimits.RenderLead));
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
		const auto ProfilingActions = DrawProfilingControls(InGui, InMetrics.Profiling);
		Actions.ProfilingMask = ProfilingActions.ProfilingMask;
		Actions.Sampling = ProfilingActions.Sampling;
		Actions.ProfilingBounds = ProfilingActions.ProfilingBounds;
		InGui.Separator();
		DrawPipelineControls(InGui, InSettings, InMetrics);
		InGui.Text("Scene targets: " + Fixed(InMetrics.SceneTargetBytes / 1048576.0) + " MiB");
		if (InMetrics.LegacyDisplayItems)
		{
			InGui.TextWrapped(
			    std::to_string(InMetrics.LegacyDisplayItems) +
			    " legacy material items render as display overlays. Add an HDR usage for scene lighting.");
		}
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
		InGui.Text("Main tick interval includes pipeline waits.");
	}
	InGui.EndPanel();
	return Actions;
}

struct FDebugUiPlugin::FImpl
{
	FGuiRenderer Renderer;

	FImpl(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FImage InFont)
	    : Renderer(InDevice, InCompiler, InTasks, std::move(InFont))
	{
	}
};

FDebugUiPlugin::FDebugUiPlugin(IRHIDevice& InDevice, FShaderCompiler& InCompiler, FTaskSystem& InTasks, FImage InFont)
    : Impl(std::make_shared<FImpl>(InDevice, InCompiler, InTasks, std::move(InFont)))
{
}

FDebugUiPlugin::~FDebugUiPlugin() = default;

void FDebugUiPlugin::Start()
{
	Impl->Renderer.Start();
}

void FDebugUiPlugin::Stop() noexcept
{
	Impl->Renderer.Stop();
}

void FDebugUiPlugin::Prepare(const FGuiDrawData& InData)
{
	Impl->Renderer.Prepare(InData);
}

void FDebugUiPlugin::Build(FRenderGraph& InGraph, const FRenderFrame&)
{
	Impl->Renderer.Build(InGraph);
}

void FDebugUiPlugin::BuildDeferred(FRenderGraph& InGraph, FGuiDrawData InData)
{
	Impl->Renderer.BuildDeferred(InGraph, std::move(InData));
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
