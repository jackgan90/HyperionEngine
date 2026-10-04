#include "Hyperion/Renderer/RenderBenchmark.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>

namespace Hyperion
{
namespace
{
void WriteShadowBenchmark(std::ostream& InOutput, const FForwardPipelineStatistics& InPipeline,
                          const FDeviceStats& InDevice, const FRenderGpuTimings& InGpu)
{
	std::size_t Items{};
	std::size_t Draws{};
	std::size_t Failed{};
	std::uint64_t Uploads{};
	double ShadowMaterial{};
	double ShadowPlan{};
	double ShadowPrepare{};
	for (const auto& View : InPipeline.Views)
	{
		if (View.StatsCategory == ERenderViewStatsCategory::Shadow)
		{
			Items += View.Visibility.VisibleItems;
			Draws += View.Visibility.Draws;
			Failed += View.Visibility.Batches.FailedItems;
			Uploads += View.Visibility.Batches.UploadBytes;
			ShadowMaterial += View.Visibility.MaterialMilliseconds;
			ShadowPlan += View.Visibility.Batches.PlanningMilliseconds;
			ShadowPrepare += View.Visibility.Batches.PreparationMilliseconds;
		}
	}
	InOutput << ',' << InPipeline.bShadows << ',' << InPipeline.PreparationMilliseconds << ','
	         << InPipeline.ShadowSetupMilliseconds << ',' << Items << ',' << Draws << ',' << Failed << ',' << Uploads
	         << ',' << InPipeline.ShadowTextureBytes << ',' << InDevice.GpuTiming.Frame << ',' << InGpu.Shadow << ','
	         << InGpu.Forward;
	for (const auto Time : InGpu.Cascades)
	{
		InOutput << ',' << Time;
	}
	InOutput << ',' << InDevice.GpuAllocationBytes << ',' << InDevice.DescriptorAllocations << ','
	         << InDevice.PipelinesCreated << ',' << InDevice.ConstantBytesWritten << ',' << ShadowMaterial << ','
	         << ShadowPlan << ',' << ShadowPrepare;
}

void WriteScenePipelineBenchmark(std::ostream& InOutput, const FForwardPipelineStatistics& InPipeline,
                                 const FRenderGpuTimings& InGpu)
{
	InOutput << ',' << InGpu.DeferredBase << ',' << InGpu.Lighting << ',' << InGpu.Compatibility << ','
	         << InGpu.Transparent << ',' << InGpu.Tonemap << ',' << InGpu.Sky << ',' << InGpu.Total;
	InOutput << ',' << InPipeline.SceneTargetBytes << ',' << InPipeline.FullscreenDraws << ','
	         << InPipeline.FullscreenPreparationMilliseconds;
	InOutput << ',' << InPipeline.bContactShadows << ',' << InPipeline.HierarchicalDepth.Consumers << ','
	         << InPipeline.HierarchicalDepth.Dispatches << ',' << InPipeline.HierarchicalDepth.Bytes << ','
	         << InGpu.HierarchicalDepth << ',' << InGpu.ContactShadow;
}

void WritePreparationBenchmark(std::ostream& InOutput, const FForwardPipelineStatistics& InPipeline)
{
	std::size_t SharedUpdates{};
	std::size_t SharedGroups{};
	std::size_t RetainedMaterials{};
	std::size_t RetainedItems{};
	std::size_t RestoredItems{};
	std::size_t ItemReuses{};
	std::size_t StorageReuses{};
	std::size_t CollectionReuses{};
	std::size_t PreparationReuses{};
	std::size_t MembershipReuses{};
	std::size_t MembershipAdded{};
	std::size_t MembershipRemoved{};
	std::size_t ContainedItems{};
	FRenderBatchStats Packing;
	for (const auto& View : InPipeline.Views)
	{
		SharedUpdates += View.Visibility.SharedMaterialUpdates;
		SharedGroups += View.Visibility.SharedMaterialGroups;
		RetainedMaterials += View.Visibility.RetainedMaterialItems;
		RetainedItems += View.Visibility.RetainedSceneItems;
		RestoredItems += View.Visibility.RetainedItemRestores;
		ItemReuses += View.Visibility.ItemPreparationReuses;
		StorageReuses += View.Visibility.ItemStorageReuses;
		CollectionReuses += View.Visibility.CollectionReuses;
		PreparationReuses += View.Visibility.PreparationReuses;
		MembershipReuses += View.Visibility.MembershipReuses;
		MembershipAdded += View.Visibility.MembershipAdded;
		MembershipRemoved += View.Visibility.MembershipRemoved;
		ContainedItems += View.Visibility.ContainedItemTests;
		Packing += View.Visibility.Batches;
	}
	InOutput << ',' << SharedUpdates << ',' << ItemReuses << ',' << StorageReuses << ',' << CollectionReuses << ','
	         << PreparationReuses << ',' << Packing.PackedRecords << ',' << Packing.ReusedRecords << ','
	         << Packing.AssembledBlocks << ',' << Packing.ReusedBlocks << ',' << Packing.AssembledBytes << ','
	         << Packing.PreparedInputBuilds << ',' << Packing.PreparedInputReuses << ','
	         << Packing.InstanceContractBuilds << ',' << Packing.CachedInputs << ',' << Packing.CachedInputBytes;
	InOutput << ',' << Packing.LocalInputReuses << ',' << Packing.LocalCompatibilityReuses << ','
	         << Packing.LocalRecordReuses;
	InOutput << ',' << SharedGroups << ',' << RetainedMaterials << ',' << Packing.LocalPlanReuses << ','
	         << Packing.LocalPacketReuses << ',' << RetainedItems << ',' << RestoredItems;
	InOutput << ',' << MembershipReuses << ',' << MembershipAdded << ',' << MembershipRemoved << ',' << ContainedItems;
	InOutput << ',' << Packing.IncrementalPlanUpdates << ',' << Packing.IncrementalItemReuses << ','
	         << Packing.AffectedBatches << ',' << Packing.RetainedBatches << ',' << Packing.BatchAdmissionReuses << ','
	         << Packing.CachedPlanItems << ',' << Packing.CachedPlanBlocks;
}
} // namespace

void MatchBenchmarkTimings(std::span<FRenderBenchmarkSample> InSamples, FGpuTimingCapture InCapture)
{
	if (InCapture.DroppedFrames || InCapture.Frames.empty())
	{
		throw std::runtime_error("Incomplete GPU timing capture");
	}
	auto& Timings = InCapture.Frames;
	std::sort(Timings.begin(), Timings.end(),
	          [](const auto& InA, const auto& InB)
	          {
		          return InA.Frame < InB.Frame;
	          });
	for (std::size_t Index = 0; Index < Timings.size(); ++Index)
	{
		if (Timings[Index].Swapchain != Timings.front().Swapchain ||
		    (Index && Timings[Index].Frame == Timings[Index - 1].Frame))
		{
			throw std::runtime_error("Ambiguous GPU timing submission identity");
		}
	}
	for (auto& Frame : InSamples)
	{
		const auto Expected = Frame.Device.SubmittedFrames;
		const auto Found = std::lower_bound(Timings.begin(), Timings.end(), Expected,
		                                    [](const auto& InTiming, std::uint64_t InFrame)
		                                    {
			                                    return InTiming.Frame < InFrame;
		                                    });
		if (!Expected || Found == Timings.end() || Found->Frame != Expected)
		{
			throw std::runtime_error("Benchmark interval contains an untimed or unsubmitted frame");
		}
		Frame.Device.GpuTiming = std::move(*Found);
	}
}

void WriteRenderBenchmark(const std::filesystem::path& InPath, std::span<const FRenderBenchmarkSample> InSamples)
{
	std::ofstream Output(InPath);
	Output.exceptions(std::ios::failbit | std::ios::badbit);
	Output << "frame,frame_ms,scene_draws,visible_items,instanced_items,instanced_draws,single_draws,failed_items,"
	          "reused_chunks,rebuilt_chunks,packed_bytes,instance_upload_bytes,gpu_reuses,compat_reuses,compat_builds,"
	          "plan_ms,prepare_ms";
	for (std::size_t Index = 0; Index < static_cast<std::size_t>(ERenderBatchFallback::Count); ++Index)
	{
		Output << ",fallback_" << GetRenderBatchFallbackName(static_cast<ERenderBatchFallback>(Index));
	}
	Output
	    << ",shadows,pipeline_prepare_ms,shadow_setup_ms,shadow_items,shadow_draws,shadow_failed,shadow_upload_bytes,"
	       "shadow_payload_bytes,gpu_sample_frame,shadow_gpu_ms,forward_gpu_ms,cascade0_gpu_ms,cascade1_gpu_ms,"
	       "cascade2_gpu_ms,cascade3_gpu_ms,gpu_allocation_bytes,descriptor_allocations,pipelines_created,constant_"
	       "bytes_written,"
	       "shadow_material_ms,shadow_plan_ms,shadow_prepare_ms";
	Output
	    << ",plan_reuses,material_ms,native_lists_created,native_list_resets,pipeline_binds,geometry_binds,dynamic_"
	       "binds,shared_material_updates,item_preparation_reuses,item_storage_reuses,collection_reuses,"
	       "view_preparation_reuses,packed_records,reused_records,assembled_blocks,reused_blocks,assembled_bytes,"
	       "batch_input_builds,batch_input_reuses,batch_contract_builds,batch_cached_inputs,batch_input_bytes"
	    << ",local_input_reuses,local_compatibility_reuses,local_record_reuses"
	    << ",shared_material_groups,retained_material_items,local_plan_reuses,local_packet_reuses,retained_scene_"
	       "items,retained_item_restores"
	    << ",membership_reuses,membership_added,membership_removed,contained_item_tests"
	    << ",incremental_plan_updates,incremental_item_reuses,affected_batches,retained_batches,batch_admission_reuses,"
	       "cached_plan_items,cached_plan_blocks,cpu_latency_ms,main_render_lead,render_rhi_lead"
	       ",base_gpu_ms,lighting_gpu_ms,compatibility_gpu_ms,transparent_gpu_ms,tonemap_gpu_ms,sky_gpu_ms,total_gpu_"
	       "pass_ms"
	       ",scene_target_bytes,fullscreen_draws,fullscreen_prepare_ms,contact_active,hzb_consumers,hzb_dispatches,hzb_"
	       "bytes,hzb_gpu_ms,contact_gpu_ms,index_rebuilds,index_refits"
	       ",local_lights_active,point_lights,spot_lights,local_visible,local_draws,local_query_ms,local_rebuilds,"
	       "local_refits,local_gpu_ms,clustered_lighting,cluster_cells,cluster_occupied,cluster_references,cluster_max_"
	       "lights,cluster_bytes,cluster_build_ms,cluster_rebuilt,nodes,scene_ms,gui_ms,render_wait_ms,viewport_width,"
	       "viewport_height,load_ms,preparation_ms,shadow_bytes,capture_x,capture_y,capture_width,capture_height\n"
	    << std::fixed << std::setprecision(6);

	for (const auto& Frame : InSamples)
	{
		Output << Frame.Frame << ',' << Frame.Milliseconds << ',' << Frame.Draws << ',' << Frame.VisibleItems << ','
		       << Frame.Batches.InstancedItems << ',' << Frame.Batches.InstancedDraws << ','
		       << Frame.Batches.SingleDraws << ',' << Frame.Batches.FailedItems << ',' << Frame.Batches.ReusedChunks
		       << ',' << Frame.Batches.RebuiltChunks << ',' << Frame.Batches.PackedBytes << ','
		       << Frame.Batches.UploadBytes << ',' << Frame.Batches.GpuReuses << ','
		       << Frame.Batches.CompatibilityReuses << ',' << Frame.Batches.CompatibilityBuilds << ','
		       << Frame.Batches.PlanningMilliseconds << ',' << Frame.Batches.PreparationMilliseconds;
		for (const auto Count : Frame.Batches.Fallbacks)
		{
			Output << ',' << Count;
		}
		const auto Gpu = AggregateRenderGpuTimings(Frame.Device.GpuTiming);
		WriteShadowBenchmark(Output, Frame.Pipeline, Frame.Device, Gpu);
		double MaterialTime{};
		for (const auto& View : Frame.Pipeline.Views)
		{
			MaterialTime += View.Visibility.MaterialMilliseconds;
		}
		Output << ',' << Frame.Batches.PlanReuses << ',' << MaterialTime;
		Output << ',' << Frame.Device.CommandListsCreated << ',' << Frame.Device.CommandListResets << ','
		       << Frame.Device.GraphicsPipelineBinds << ',' << Frame.Device.GraphicsGeometryBinds << ','
		       << Frame.Device.GraphicsDynamicBinds;
		WritePreparationBenchmark(Output, Frame.Pipeline);
		Output << ',' << Frame.CpuLatencyMilliseconds << ',' << Frame.MainRenderLead << ',' << Frame.RenderRhiLead;
		WriteScenePipelineBenchmark(Output, Frame.Pipeline, Gpu);
		Output << ',' << Frame.Pipeline.Spatial.IndexRebuilds << ',' << Frame.Pipeline.Spatial.IndexRefits;
		const auto& Lights = Frame.Pipeline.LocalLights;
		Output << ',' << Lights.bActive << ',' << Lights.Points << ',' << Lights.Spots << ','
		       << Lights.VisiblePoints + Lights.VisibleSpots << ',' << Lights.Draws << ','
		       << Lights.Spatial.QueryMilliseconds << ',' << Lights.Spatial.IndexRebuilds << ','
		       << Lights.Spatial.IndexRefits << ',' << Gpu.LocalLights << ',' << Lights.bClustered << ','
		       << Lights.ClusterCells << ',' << Lights.ClusterOccupied << ',' << Lights.ClusterReferences << ','
		       << Lights.ClusterMaximum << ',' << Lights.ClusterBytes << ',' << Lights.ClusterBuildMilliseconds << ','
		       << Lights.bClusterRebuilt << ',' << Frame.Nodes << ',' << Frame.SceneMilliseconds << ','
		       << Frame.GuiMilliseconds << ',' << Frame.RenderMilliseconds << ',' << Frame.Viewport.Width << ','
		       << Frame.Viewport.Height << ',' << Frame.LoadMilliseconds << ','
		       << Frame.Pipeline.PreparationMilliseconds << ',' << Frame.Pipeline.ShadowTextureBytes << ','
		       << Frame.CaptureViewport.X << ',' << Frame.CaptureViewport.Y << ',' << Frame.CaptureViewport.Width << ','
		       << Frame.CaptureViewport.Height << '\n';
	}
	Output.close();
}

std::array<FInputEvent, 3> BenchmarkCameraInput(std::uint64_t InFrame, float InStep)
{
	const auto Phase = InFrame % 40;
	const auto Previous = (InFrame + 39) % 40;
	std::array<FInputEvent, 3> Events;
	Events[0].Type = EEventType::MouseButton;
	Events[0].Button = 1;
	Events[0].bDown = true;
	Events[0].X = InFrame == 0 ? 0.f : float(Previous < 20 ? Previous : 40 - Previous) * InStep;
	Events[1].Type = EEventType::MouseMove;
	Events[1].X = float(Phase < 20 ? Phase : 40 - Phase) * InStep;
	Events[2].Type = EEventType::MouseButton;
	Events[2].Button = 1;
	Events[2].X = Events[1].X;
	return Events;
}

FVec3 BenchmarkLightDirection(std::uint64_t InFrame)
{
	const float Phase = float(InFrame) * .004f;
	return Normalize(FVec3{std::sin(Phase), .8f + .15f * std::sin(Phase * .7f), std::cos(Phase)});
}
} // namespace Hyperion
