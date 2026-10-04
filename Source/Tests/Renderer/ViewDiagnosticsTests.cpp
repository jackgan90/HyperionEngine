#include "Hyperion/Reflection/Json.h"
#include "Hyperion/Reflection/Wire.h"
#include "Hyperion/Renderer/RenderBenchmark.h"
#include "Hyperion/Renderer/RenderDiagnostics.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>

using namespace Hyperion;

namespace
{
constexpr std::string_view BenchmarkHeader =
    "frame,frame_ms,scene_draws,visible_items,instanced_items,instanced_draws,single_draws,failed_items,"
    "reused_chunks,rebuilt_chunks,packed_bytes,instance_upload_bytes,gpu_reuses,compat_reuses,compat_builds,"
    "plan_ms,prepare_ms,fallback_disabled,fallback_shader,fallback_device,fallback_ordering,fallback_singleton,"
    "fallback_preparation,shadows,pipeline_prepare_ms,shadow_setup_ms,shadow_items,shadow_draws,shadow_failed,"
    "shadow_upload_bytes,shadow_payload_bytes,gpu_sample_frame,shadow_gpu_ms,forward_gpu_ms,cascade0_gpu_ms,"
    "cascade1_gpu_ms,cascade2_gpu_ms,cascade3_gpu_ms,gpu_allocation_bytes,descriptor_allocations,pipelines_created,"
    "constant_bytes_written,shadow_material_ms,shadow_plan_ms,shadow_prepare_ms,plan_reuses,material_ms,"
    "native_lists_created,native_list_resets,pipeline_binds,geometry_binds,dynamic_binds,shared_material_updates,"
    "item_preparation_reuses,item_storage_reuses,collection_reuses,view_preparation_reuses,packed_records,"
    "reused_records,assembled_blocks,reused_blocks,assembled_bytes,batch_input_builds,batch_input_reuses,"
    "batch_contract_builds,batch_cached_inputs,batch_input_bytes,local_input_reuses,local_compatibility_reuses,"
    "local_record_reuses,shared_material_groups,retained_material_items,local_plan_reuses,local_packet_reuses,"
    "retained_scene_items,retained_item_restores,membership_reuses,membership_added,membership_removed,"
    "contained_item_tests,incremental_plan_updates,incremental_item_reuses,affected_batches,retained_batches,"
    "batch_admission_reuses,cached_plan_items,cached_plan_blocks,cpu_latency_ms,main_render_lead,render_rhi_lead,"
    "base_gpu_ms,lighting_gpu_ms,compatibility_gpu_ms,transparent_gpu_ms,tonemap_gpu_ms,sky_gpu_ms,total_gpu_pass_ms,"
    "scene_target_bytes,fullscreen_draws,fullscreen_prepare_ms,contact_active,hzb_consumers,hzb_dispatches,"
    "hzb_bytes,hzb_gpu_ms,contact_gpu_ms,index_rebuilds,index_refits,local_lights_active,point_lights,spot_lights,"
    "local_visible,local_draws,local_query_ms,local_rebuilds,local_refits,local_gpu_ms,clustered_lighting,cluster_"
    "cells,"
    "cluster_occupied,cluster_references,cluster_max_lights,cluster_bytes,cluster_build_ms,cluster_rebuilt,nodes,"
    "scene_ms,gui_ms,render_wait_ms,viewport_width,viewport_height,load_ms,preparation_ms,shadow_bytes,capture_x,"
    "capture_y,capture_width,capture_height";

FSceneVisibilityStats Visibility(std::size_t InSeed)
{
	FSceneVisibilityStats Result;
	Result.Groups = InSeed + 1;
	Result.Primitives = InSeed + 2;
	Result.VisibleItems = InSeed + 1;
	Result.Draws = InSeed + 2;
	Result.CollectionReuses = InSeed + 3;
	Result.PreparationReuses = InSeed + 4;
	Result.PacketReuses = InSeed + 5;
	Result.UpdateMilliseconds = double(InSeed) + .5;
	Result.QueryMilliseconds = double(InSeed) + .25;
	Result.MaterialMilliseconds = double(InSeed) + .75;
	Result.Batches.SingleDraws = InSeed + 3;
	Result.Batches.FailedItems = InSeed + 4;
	Result.Batches.CachedChunks = InSeed + 5;
	Result.Batches.UploadBytes = InSeed * 100;
	Result.Batches.PlanningMilliseconds = double(InSeed) + .125;
	Result.Batches.PreparationMilliseconds = double(InSeed) + .5;
	return Result;
}

FForwardPipelineStatistics DiagnosticPipeline()
{
	FForwardPipelineStatistics Result;
	Result.PreparationMilliseconds = 1.25;
	Result.ShadowSetupMilliseconds = 2.5;
	Result.FullscreenPreparationMilliseconds = 3.75;
	Result.SceneTargetBytes = 4096;
	Result.FullscreenDraws = 5;
	Result.Spatial = Visibility(40);
	Result.Views = {{101, "ShadowDepth", Visibility(100), ERenderViewStatsCategory::Shadow},
	                {1, "Forward", Visibility(10)},
	                {2, "CustomShadowTest", Visibility(20)},
	                {102, "ShadowDepth", Visibility(200), ERenderViewStatsCategory::Shadow}};
	Result.ShadowTextureBytes = 8192;
	Result.bShadows = true;
	Result.bContactShadows = true;
	Result.HierarchicalDepth = {2, 1, 7, 2048};
	Result.LocalLights.Points = 3;
	Result.LocalLights.Spots = 4;
	Result.LocalLights.bActive = true;
	Result.SelectionOutline.Objects = 6;
	Result.CameraStatus = ESceneCameraStatus::DefaultFallback;
	return Result;
}

void CheckMainAggregation(const FForwardPipelineStatistics& InPipeline)
{
	const auto Main = InPipeline.MainView();
	HYP_CHECK(Main.VisibleItems == 32 && Main.Draws == 34);
	HYP_CHECK(Main.Groups == 11 && Main.Primitives == 12);
	HYP_CHECK(Main.UpdateMilliseconds == 10.5 && Main.QueryMilliseconds == 10.25);
	HYP_CHECK(Main.MaterialMilliseconds == 10.75 && Main.CollectionReuses == 13);
	HYP_CHECK(Main.PreparationReuses == 14 && Main.PacketReuses == 15);
	HYP_CHECK(Main.Batches.SingleDraws == 36 && Main.Batches.FailedItems == 38);
	HYP_CHECK(Main.Batches.UploadBytes == 3000 && Main.Batches.CachedChunks == 25);
	HYP_CHECK(Main.Batches.PlanningMilliseconds == 30.25 && Main.Batches.PreparationMilliseconds == 31);
}

void CheckDiagnosticDescriptors()
{
	const auto& View = RecordType<FRenderViewStatistics>();
	const std::array<std::string_view, 3> ViewKeys{"identity", "usage", "visibility"};
	HYP_CHECK(View.Id == "hyperion.diagnostics.renderviewstatistics" && View.Version == 1);
	HYP_CHECK(View.Members.size() == ViewKeys.size());
	for (std::size_t Index = 0; Index < ViewKeys.size(); ++Index)
	{
		HYP_CHECK(View.Members[Index].Id == ViewKeys[Index]);
	}
	const auto& Pipeline = RecordType<FForwardPipelineStatistics>();
	const std::array<std::string_view, 14> PipelineKeys{"preparationMilliseconds",
	                                                    "shadowSetupMilliseconds",
	                                                    "fullscreenPreparationMilliseconds",
	                                                    "sceneTargetBytes",
	                                                    "fullscreenDraws",
	                                                    "spatial",
	                                                    "localLights",
	                                                    "selectionOutline",
	                                                    "views",
	                                                    "shadowTextureBytes",
	                                                    "shadows",
	                                                    "contactShadows",
	                                                    "hierarchicalDepth",
	                                                    "cameraStatus"};
	HYP_CHECK(Pipeline.Id == "hyperion.diagnostics.forwardpipelinestatistics" && Pipeline.Version == 1);
	HYP_CHECK(Pipeline.Members.size() == PipelineKeys.size());
	for (std::size_t Index = 0; Index < PipelineKeys.size(); ++Index)
	{
		HYP_CHECK(Pipeline.Members[Index].Id == PipelineKeys[Index]);
	}
}

std::string ReadBytes(const std::filesystem::path& InPath)
{
	std::ifstream Input(InPath, std::ios::binary);
	HYP_CHECK(Input.good());
	return {std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
}

void CheckOrCapture(const std::filesystem::path& InCaptureDirectory, std::string_view InName,
                    const std::string& InBytes)
{
	if (InCaptureDirectory.empty())
	{
		const auto Path = std::filesystem::path(HYP_SOURCE_DIR) / "Source/Tests/Fixtures/ViewDiagnostics" / InName;
		HYP_CHECK(ReadBytes(Path) == InBytes);
		return;
	}
	std::filesystem::create_directories(InCaptureDirectory);
	const auto Path = InCaptureDirectory / InName;
	if (std::filesystem::exists(Path))
	{
		throw std::runtime_error("Diagnostic capture never overwrites an existing oracle: " + Path.string());
	}
	std::ofstream Output(Path, std::ios::binary);
	Output.exceptions(std::ios::failbit | std::ios::badbit);
	Output << InBytes;
	Output.close();
}

FRenderBenchmarkSample BenchmarkSample(const FForwardPipelineStatistics& InPipeline)
{
	FRenderBenchmarkSample Result;
	Result.Frame = 77;
	Result.Milliseconds = 4.125;
	Result.Pipeline = InPipeline;
	Result.Draws = 34;
	Result.VisibleItems = 32;
	Result.Batches = InPipeline.MainView().Batches;
	Result.Device.GpuTiming.Frame = 88;
	Result.Device.GpuTiming.Passes = {
	    {"Shadow cascade 0/0", 1, EncodeRenderPassTiming({ERenderPassTimingCategory::Shadow, 0})},
	    {"Shadow cascade 0/1", .5, EncodeRenderPassTiming({ERenderPassTimingCategory::Shadow, 0})},
	    {"Shadow cascade 1/0", 2, EncodeRenderPassTiming({ERenderPassTimingCategory::Shadow, 1})},
	    {"Shadow cascade 2/0", 3, EncodeRenderPassTiming({ERenderPassTimingCategory::Shadow, 2})},
	    {"Shadow cascade 3/0", 4, EncodeRenderPassTiming({ERenderPassTimingCategory::Shadow, 3})},
	    {"Forward/HDR/0", 5.25, EncodeRenderPassTiming({ERenderPassTimingCategory::Forward})},
	    {"Deferred/BasePass/0", 11, EncodeRenderPassTiming({ERenderPassTimingCategory::DeferredBase})},
	    {"Deferred/Lighting/0", 12, EncodeRenderPassTiming({ERenderPassTimingCategory::Lighting})},
	    {"Deferred/Compatibility/0", 13, EncodeRenderPassTiming({ERenderPassTimingCategory::Compatibility})},
	    {"Scene/Transparent/0", 14, EncodeRenderPassTiming({ERenderPassTimingCategory::Transparent})},
	    {"Output/Tonemap/0", 15, EncodeRenderPassTiming({ERenderPassTimingCategory::Tonemap})},
	    {"Scene/Sky/0", 16, EncodeRenderPassTiming({ERenderPassTimingCategory::Sky})},
	    {"HZB/0", 17, EncodeRenderPassTiming({ERenderPassTimingCategory::HierarchicalDepth})},
	    {"Deferred/ContactShadowMask/0", 18, EncodeRenderPassTiming({ERenderPassTimingCategory::ContactShadow})},
	    {"Deferred/LocalLights/0", 19, EncodeRenderPassTiming({ERenderPassTimingCategory::LocalLights})},
	    {"Deferred/LightingClustered/0", 20, EncodeRenderPassTiming({ERenderPassTimingCategory::Lighting})},
	    {"Deferred/ClusterLighting/0", 21, EncodeRenderPassTiming({ERenderPassTimingCategory::Lighting})},
	    {"ShadowDepth/0", 901},
	    {"Forwardish/0", 902}};
	Result.Viewport = {320, 240};
	Result.CaptureViewport = {4, 8, 160, 120};
	return Result;
}

void CheckRenamedBenchmark(const FRenderBenchmarkSample& InSample, const std::filesystem::path& InPath)
{
	const auto Original = ReadBytes(InPath);
	auto Renamed = InSample;
	for (std::size_t Index = 0; Index < Renamed.Device.GpuTiming.Passes.size(); ++Index)
	{
		auto& Pass = Renamed.Device.GpuTiming.Passes[Index];
		Pass.Name = "Renamed pass " + std::to_string(Index);
		if (!DecodeRenderPassTiming(Pass.Tag))
		{
			Pass.Name = "Shadow cascade 0/Forward/" + std::to_string(Index);
		}
	}
	WriteRenderBenchmark(InPath, std::span(&Renamed, 1));
	HYP_CHECK(ReadBytes(InPath) == Original);
}

std::vector<std::string> CsvCells(const std::string& InLine)
{
	std::istringstream Input(InLine);
	std::vector<std::string> Result;
	std::string Cell;
	while (std::getline(Input, Cell, ','))
	{
		Result.push_back(Cell);
	}
	return Result;
}

std::string CheckBenchmark(const FForwardPipelineStatistics& InPipeline)
{
	const auto Sample = BenchmarkSample(InPipeline);
	const auto Path = std::filesystem::absolute("ViewDiagnosticsActual.csv");
	WriteRenderBenchmark(Path, std::span(&Sample, 1));
	std::ifstream Input(Path);
	std::string Header;
	std::string Row;
	std::string Extra;
	HYP_CHECK(std::getline(Input, Header) && std::getline(Input, Row) && !std::getline(Input, Extra));
	HYP_CHECK(Header == BenchmarkHeader);
	const auto Keys = CsvCells(Header);
	const auto Values = CsvCells(Row);
	HYP_CHECK(Keys.size() == 138 && Values.size() == 138);
	const std::array<std::pair<std::string_view, std::string_view>, 30> Expected{{{"frame", "77"},
	                                                                              {"frame_ms", "4.125000"},
	                                                                              {"scene_draws", "34"},
	                                                                              {"visible_items", "32"},
	                                                                              {"shadow_items", "302"},
	                                                                              {"shadow_draws", "304"},
	                                                                              {"shadow_failed", "308"},
	                                                                              {"shadow_upload_bytes", "30000"},
	                                                                              {"shadow_material_ms", "301.500000"},
	                                                                              {"shadow_plan_ms", "300.250000"},
	                                                                              {"shadow_prepare_ms", "301.000000"},
	                                                                              {"material_ms", "333.000000"},
	                                                                              {"gpu_sample_frame", "88"},
	                                                                              {"shadow_gpu_ms", "10.500000"},
	                                                                              {"forward_gpu_ms", "5.250000"},
	                                                                              {"cascade0_gpu_ms", "1.500000"},
	                                                                              {"cascade1_gpu_ms", "2.000000"},
	                                                                              {"cascade2_gpu_ms", "3.000000"},
	                                                                              {"cascade3_gpu_ms", "4.000000"},
	                                                                              {"base_gpu_ms", "11.000000"},
	                                                                              {"lighting_gpu_ms", "53.000000"},
	                                                                              {"compatibility_gpu_ms", "13.000000"},
	                                                                              {"transparent_gpu_ms", "14.000000"},
	                                                                              {"tonemap_gpu_ms", "15.000000"},
	                                                                              {"sky_gpu_ms", "16.000000"},
	                                                                              {"hzb_gpu_ms", "17.000000"},
	                                                                              {"contact_gpu_ms", "18.000000"},
	                                                                              {"local_gpu_ms", "19.000000"},
	                                                                              {"pipeline_prepare_ms", "1.250000"},
	                                                                              {"shadow_payload_bytes", "8192"}}};
	for (const auto& Entry : Expected)
	{
		const auto Found = std::find(Keys.begin(), Keys.end(), Entry.first);
		HYP_CHECK(Found != Keys.end());
		HYP_CHECK(Values.at(static_cast<std::size_t>(Found - Keys.begin())) == Entry.second);
	}
	Input.close();
	CheckRenamedBenchmark(Sample, Path);
	return Header + '\n' + Row + '\n';
}

void CheckCategoryAggregation()
{
	FForwardPipelineStatistics Pipeline;
	Pipeline.Views = {{1, "ShadowDepth", Visibility(10)},
	                  {2, "CustomShadowTest", Visibility(20), ERenderViewStatsCategory::Shadow},
	                  {3, "Forward", Visibility(900), ERenderViewStatsCategory::Uncounted},
	                  {4, "OtherMain", Visibility(30)}};
	const auto Main = Pipeline.MainView();
	HYP_CHECK(Main.VisibleItems == 42 && Main.Draws == 44 && Main.Batches.SingleDraws == 46);
	HYP_CHECK(Main.Groups == 11 && Main.QueryMilliseconds == 10.25 && Main.MaterialMilliseconds == 10.75);
	const auto& Type = RecordType<FRenderViewStatistics>();
	const auto OriginalWire = WriteJson(WriteRecordWire(Type, &Pipeline.Views[0]));
	const auto Loaded = ReadRecordWire(Type, WriteRecordWire(Type, &Pipeline.Views[1]));
	HYP_CHECK(static_cast<const FRenderViewStatistics*>(Loaded.get())->StatsCategory == ERenderViewStatsCategory::Main);
	const std::array<std::string_view, 7> Fields{"shadow_items",        "shadow_draws",       "shadow_failed",
	                                             "shadow_upload_bytes", "shadow_material_ms", "shadow_plan_ms",
	                                             "shadow_prepare_ms"};
	const std::array<std::array<std::string_view, 7>, 3> Expected{
	    {{"21", "22", "24", "2000", "20.750000", "20.125000", "20.500000"},
	     {"32", "34", "38", "3000", "31.500000", "30.250000", "31.000000"},
	     {"21", "22", "24", "2000", "20.750000", "20.125000", "20.500000"}}};
	const std::array Categories{ERenderViewStatsCategory::Main, ERenderViewStatsCategory::Shadow,
	                            ERenderViewStatsCategory::Uncounted};
	for (std::size_t Index = 0; Index < Categories.size(); ++Index)
	{
		Pipeline.Views[0].StatsCategory = Categories[Index];
		const auto Current = Pipeline.MainView();
		HYP_CHECK(Current.VisibleItems == (Index == 0 ? 42 : 31));
		HYP_CHECK(Current.Groups == (Index == 0 ? 11 : 31));
		HYP_CHECK(WriteJson(WriteRecordWire(Type, &Pipeline.Views[0])) == OriginalWire);
		const auto Sample = BenchmarkSample(Pipeline);
		const auto Path = std::filesystem::absolute("ViewPolicyDiagnosticsActual.csv");
		WriteRenderBenchmark(Path, std::span(&Sample, 1));
		std::ifstream Input(Path);
		std::string Header;
		std::string Row;
		HYP_CHECK(std::getline(Input, Header) && std::getline(Input, Row));
		HYP_CHECK(Header == BenchmarkHeader);
		const auto Keys = CsvCells(Header);
		const auto Values = CsvCells(Row);
		HYP_CHECK(Keys.size() == 138 && Values.size() == 138);
		for (std::size_t Field = 0; Field < Fields.size(); ++Field)
		{
			const auto Found = std::find(Keys.begin(), Keys.end(), Fields[Field]);
			HYP_CHECK(Found != Keys.end());
			HYP_CHECK(Values.at(static_cast<std::size_t>(Found - Keys.begin())) == Expected[Index][Field]);
		}
	}
}
} // namespace

void RunViewDiagnosticsTests(const std::filesystem::path& InCaptureDirectory)
{
	CheckDiagnosticDescriptors();
	CheckCategoryAggregation();
	const auto Pipeline = DiagnosticPipeline();
	CheckMainAggregation(Pipeline);
	const auto Csv = CheckBenchmark(Pipeline);
	const auto& ViewType = RecordType<FRenderViewStatistics>();
	const auto& PipelineType = RecordType<FForwardPipelineStatistics>();
	CheckOrCapture(InCaptureDirectory, "RenderViewStatisticsSchema.json", WriteJson(RecordWireSchema(ViewType)));
	CheckOrCapture(InCaptureDirectory, "RenderViewStatisticsWire.json",
	               WriteJson(WriteRecordWire(ViewType, &Pipeline.Views.front())));
	CheckOrCapture(InCaptureDirectory, "ForwardPipelineStatisticsSchema.json",
	               WriteJson(RecordWireSchema(PipelineType)));
	CheckOrCapture(InCaptureDirectory, "ForwardPipelineStatisticsWire.json",
	               WriteJson(WriteRecordWire(PipelineType, &Pipeline)));
	CheckOrCapture(InCaptureDirectory, "RenderViewBenchmark.csv", Csv);
}
