#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/RasterOptions/RasterOptions.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Hyperion
{
namespace
{
EGBufferVisualizer DisplayedVisualizer(const FRenderSettings& InSettings)
{
	return ParseSceneRenderPipeline(InSettings.Pipeline) == ESceneRenderPipeline::Deferred
	           ? ParseGBufferVisualizer(InSettings.DebugMode)
	           : EGBufferVisualizer::Lit;
}

std::string Number(double InValue)
{
	std::ostringstream Text;
	Text << std::fixed << std::setprecision(2) << InValue;
	return Text.str();
}

void AddViewLines(std::vector<std::string>& InLines, const FRenderDiagnostics& InData)
{
	InLines.push_back("RENDER VIEWS");
	for (const auto& View : InData.Pipeline.Views)
	{
		const auto& Stats = View.Visibility;
		InLines.push_back(View.Usage + ": " + std::to_string(Stats.VisibleItems) + " items / " +
		                  std::to_string(Stats.Draws) + " draws");
		InLines.push_back("BVH " + std::to_string(Stats.VisitedNodes) + " visits / " +
		                  std::to_string(Stats.CandidatePrimitives) + " candidates");
		InLines.push_back("Instanced " + std::to_string(Stats.Batches.InstancedDraws) + " / upload " +
		                  std::to_string(Stats.Batches.UploadBytes) + " B");
	}
}

void AddVisibilityLines(std::vector<std::string>& InLines, const FSceneVisibilityStats& InStats)
{
	InLines.push_back("MAIN VIEW VISIBILITY");
	InLines.push_back("Groups " + std::to_string(InStats.Groups) + " / unbounded " +
	                  std::to_string(InStats.UnboundedGroups));
	InLines.push_back("BVH visits " + std::to_string(InStats.VisitedNodes) + " / tests " +
	                  std::to_string(InStats.GroupTests));
	InLines.push_back("Primitives " + std::to_string(InStats.Primitives) + " / candidates " +
	                  std::to_string(InStats.CandidatePrimitives));
	InLines.push_back("Collect calls " + std::to_string(InStats.CollectedPrimitives) + " / emitted " +
	                  std::to_string(InStats.EmittedItems));
	InLines.push_back("Visible " + std::to_string(InStats.VisibleItems) + " / draws " + std::to_string(InStats.Draws));
	InLines.push_back("Membership reused " + std::to_string(InStats.MembershipReuses));
	InLines.push_back("Entered / left " + std::to_string(InStats.MembershipAdded) + " / " +
	                  std::to_string(InStats.MembershipRemoved));
	InLines.push_back("Index update / query " + Number(InStats.UpdateMilliseconds) + " / " +
	                  Number(InStats.QueryMilliseconds) + " ms");
	InLines.push_back("Rebuilds / refit leaves " + std::to_string(InStats.IndexRebuilds) + " / " +
	                  std::to_string(InStats.IndexRefits));
}

void AddBatchLines(std::vector<std::string>& InLines, const FRenderBatchStats& InStats)
{
	InLines.push_back("MAIN VIEW BATCHING");
	InLines.push_back("Batch plan / draw prep " + Number(InStats.PlanningMilliseconds) + " / " +
	                  Number(InStats.PreparationMilliseconds) + " ms");
	InLines.push_back("Instanced " + std::to_string(InStats.InstancedItems) + " in " +
	                  std::to_string(InStats.InstancedDraws) + " draws");
	InLines.push_back("Singles " + std::to_string(InStats.SingleDraws));
	InLines.push_back("Incremental updates " + std::to_string(InStats.IncrementalPlanUpdates));
	InLines.push_back("Affected / retained blocks " + std::to_string(InStats.AffectedBatches) + " / " +
	                  std::to_string(InStats.RetainedBatches));
	InLines.push_back("Admissions reused " + std::to_string(InStats.BatchAdmissionReuses));
	InLines.push_back("Cached members / blocks " + std::to_string(InStats.CachedPlanItems) + " / " +
	                  std::to_string(InStats.CachedPlanBlocks));
	InLines.push_back("Chunks reused / rebuilt " + std::to_string(InStats.ReusedChunks) + " / " +
	                  std::to_string(InStats.RebuiltChunks));
	InLines.push_back("Upload " + std::to_string(InStats.UploadBytes) + " B");
	bool bFallback{};
	for (std::size_t Index = 0; Index < InStats.Fallbacks.size(); ++Index)
	{
		if (InStats.Fallbacks[Index])
		{
			InLines.push_back("Fallback " +
			                  std::string(GetRenderBatchFallbackName(static_cast<ERenderBatchFallback>(Index))) + ": " +
			                  std::to_string(InStats.Fallbacks[Index]));
			bFallback = true;
		}
	}
	if (!bFallback)
	{
		InLines.push_back("Fallbacks: none");
	}
}

std::vector<std::string> ProfilingLines(const FRenderDiagnostics& InData, std::uint32_t InMask)
{
	std::vector<std::string> Lines;
	if (InMask & 1)
	{
		const auto Time = InData.FrameIntervalMilliseconds;
		Lines = {"OVERVIEW", Number(Time) + " ms  |  " + Number(Time > 0 ? 1000 / Time : 0) + " FPS",
		         "Frame " + std::to_string(InData.Frame) + " / GPU " + std::to_string(InData.GpuTimingFrame),
		         "CPU tracked " + Number(double(InData.TrackedCpuBytes) / 1048576) + " MiB",
		         "Scene targets " + Number(double(InData.Pipeline.SceneTargetBytes) / 1048576) + " MiB"};
	}
	if (InMask & 2)
	{
		Lines.push_back("TASKS");
		for (const auto& [Name, Count] : InData.ExecutedTasks)
		{
			Lines.push_back(Name + ": " + std::to_string(Count));
		}
	}
	if (InMask & 4)
	{
		Lines.push_back("GPU PASSES");
		if (InData.GpuPassMilliseconds.empty())
		{
			Lines.push_back("Waiting for completed GPU timings");
		}
		for (const auto& [Name, Time] : InData.GpuPassMilliseconds)
		{
			Lines.push_back(Name + ": " + Number(Time) + " ms");
		}
	}
	if (InMask & 8)
	{
		Lines.push_back("DEVICE");
		for (const auto& [Name, Count] : InData.Device)
		{
			Lines.push_back(Name + ": " + std::to_string(Count));
		}
	}
	if (InMask & 16)
	{
		AddViewLines(Lines, InData);
	}
	if (InMask & 32)
	{
		const auto& Depth = InData.Pipeline.HierarchicalDepth;
		const auto& Lights = InData.Pipeline.LocalLights;
		Lines.push_back("LIGHTING / HZB");
		Lines.push_back("HZB " + std::to_string(Depth.Consumers) + " consumers / " + std::to_string(Depth.Dispatches) +
		                " dispatches");
		Lines.push_back("HZB bytes " + std::to_string(Depth.Bytes));
		Lines.push_back("Visible point / spot " + std::to_string(Lights.VisiblePoints) + " / " +
		                std::to_string(Lights.VisibleSpots));
		Lines.push_back("Cluster references " + std::to_string(Lights.ClusterReferences));
		Lines.push_back("Cluster bytes " + std::to_string(Lights.ClusterBytes));
	}
	if (InMask & 64)
	{
		AddVisibilityLines(Lines, InData.Pipeline.MainView());
	}
	if (InMask & 128)
	{
		AddBatchLines(Lines, InData.Pipeline.MainView().Batches);
	}
	if (Lines.empty())
	{
		Lines.push_back("Choose profiling categories from Stats...");
	}
	return Lines;
}
} // namespace

void FEditorPlugin::DrawHudButtons()
{
	FSceneViewportOptions Patch;
	Gui->SameLineIfFits("Info");
	if (Gui->IconButton("##StatusHud", EGuiIcon::Information, "Toggle rendering status HUD", bShowStatusHud))
	{
		Patch.StatusHud = !bShowStatusHud;
	}
	Acceptance.ObserveWidget(EEditorWidget::HudStatusToggle, Gui->LastItemBounds());
	Gui->SameLineIfFits("Stats");
	if (Gui->IconButton("##ProfilingHud", EGuiIcon::Statistics, "Toggle profiling HUD", bShowProfilingHud))
	{
		Patch.ProfilingHud = !bShowProfilingHud;
	}
	Acceptance.ObserveWidget(EEditorWidget::HudProfilingToggle, Gui->LastItemBounds());
	SetViewportOptions(Patch);
}

void FEditorPlugin::DrawVisualizationControls()
{
	FSceneViewportOptions Patch;
	const float Width = Gui->AvailableWidth();
	const bool bCompact = Width < 300;
	const auto Visualizers = GBufferVisualizerOptions();
	static const auto Labels = RasterOptionLabels(Visualizers);
	const bool bDeferred = ParseSceneRenderPipeline(Rendering.Pipeline) == ESceneRenderPipeline::Deferred;
	auto Mode = RasterOptionIndex(Visualizers, DisplayedVisualizer(Rendering));
	Gui->SetNextItemWidth(bCompact ? std::max(40.f, Width - 70) : 150);
	Gui->BeginDisabled(!bDeferred);
	if (Gui->Combo("##Visualizer", Labels, Mode,
	               [&](std::size_t InIndex, FVec4 InBounds)
	               {
		               Acceptance.ObserveIndexedWidget(EEditorWidget::VisualizerItem, InBounds,
		                                               ToVisualizerWireValue(Visualizers[InIndex].Id));
	               }))
	{
		Patch.Visualizer = ToVisualizerWireValue(RasterOptionIdentity(Visualizers, Mode));
	}
	Gui->Tooltip(bDeferred ? "Viewport visualizer" : "GBuffer visualization requires Deferred");
	Acceptance.ObserveWidget(EEditorWidget::Visualizer, Gui->LastItemBounds());
	Gui->EndDisabled();
	Gui->SameLineIfFits("Stats...");
	if (Gui->Button("Stats..."))
	{
		Gui->OpenPopup("ProfilingOptions");
	}
	ProfilingOptionsAnchor = Gui->LastItemBounds();
	Acceptance.ObserveWidget(EEditorWidget::ProfilingCategories, ProfilingOptionsAnchor);
	Gui->SameLineIfFits("Exposure                         ");
	Gui->SetNextItemWidth(std::clamp(Width - 65, 30.f, 100.f));
	auto Value = Exposure;
	if (Gui->Slider("Exposure", Value, .05f, 8))
	{
		Patch.Exposure = Value;
	}
	Acceptance.ObserveWidget(EEditorWidget::Exposure, Gui->LastItemBounds());
	if (!bCompact)
	{
		Gui->SameLineIfFits("Editor view                     ");
		DrawViewSelector(150);
	}
	SetViewportOptions(Patch);
}

void FEditorPlugin::DrawViewportHud()
{
	Acceptance.ObserveWidget(EEditorWidget::StatusHud, {});
	Acceptance.ObserveWidget(EEditorWidget::ProfilingHud, {});
	if (!bShowStatusHud && !bShowProfilingHud)
	{
		return;
	}
	const auto Now = ClockNanoseconds();
	if (!HudUpdated || Now - HudUpdated > 100000000)
	{
		HudDiagnostics = RenderDiagnostics();
		HudUpdated = Now;
	}
	auto Left = Viewport.ViewportRegion.Bounds;
	auto Right = Left;
	if (bShowStatusHud && bShowProfilingHud)
	{
		Left.Z = Right.X = (Left.X + Left.Z) * .5f;
	}
	if (bShowStatusHud)
	{
		std::vector<std::string> Lines{
		    "RENDER STATUS",
		    "Pipeline: " + Rendering.Pipeline,
		    "GBuffer: " + (ParseSceneRenderPipeline(Rendering.Pipeline) == ESceneRenderPipeline::Deferred
		                       ? Rendering.GBuffer
		                       : std::string("not used")),
		    Rendering.bReversedZ ? "Active depth: Reversed Z" : "Active depth: Standard Z",
		    "Visualizer: " + std::string(DescribeGBufferVisualizer(DisplayedVisualizer(Rendering)).Label),
		    "Exposure: " + Number(Exposure),
		    "Viewport: " + std::to_string(Viewport.ViewportSize.Width) + " x " +
		        std::to_string(Viewport.ViewportSize.Height),
		    HudDiagnostics.Adapter};
		Acceptance.ObserveWidget(EEditorWidget::StatusHud, Gui->DrawImageText(Left, Lines));
	}
	if (bShowProfilingHud)
	{
		Acceptance.ObserveWidget(EEditorWidget::ProfilingHud,
		                         Gui->DrawImageText(Right, ProfilingLines(HudDiagnostics, ProfilingCategories), true));
	}
}
} // namespace Hyperion
