#include "Hyperion/Renderer/RenderDiagnostics.h"

namespace Hyperion
{
template<> std::span<const ESceneCameraStatus> RecordEnumValues<ESceneCameraStatus>()
{
	static constexpr std::array Values{ESceneCameraStatus::Active, ESceneCameraStatus::DefaultFallback,
	                                   ESceneCameraStatus::NoActiveCamera, ESceneCameraStatus::EmptyViewport};
	return Values;
}

template<> const FRecordDescriptor& RecordType<FRenderBatchStats>()
{
	static const auto Type =
	    MakeRecord<FRenderBatchStats>("hyperion.diagnostics.renderbatchstats",
	                                  {Member("eligibleItems", &FRenderBatchStats::EligibleItems),
	                                   Member("instancedDraws", &FRenderBatchStats::InstancedDraws),
	                                   Member("instancedItems", &FRenderBatchStats::InstancedItems),
	                                   Member("singleDraws", &FRenderBatchStats::SingleDraws),
	                                   Member("failedItems", &FRenderBatchStats::FailedItems),
	                                   Member("capacitySplits", &FRenderBatchStats::CapacitySplits),
	                                   Member("reusedChunks", &FRenderBatchStats::ReusedChunks),
	                                   Member("rebuiltChunks", &FRenderBatchStats::RebuiltChunks),
	                                   Member("packedBytes", &FRenderBatchStats::PackedBytes),
	                                   Member("packedRecords", &FRenderBatchStats::PackedRecords),
	                                   Member("reusedRecords", &FRenderBatchStats::ReusedRecords),
	                                   Member("assembledBlocks", &FRenderBatchStats::AssembledBlocks),
	                                   Member("reusedBlocks", &FRenderBatchStats::ReusedBlocks),
	                                   Member("assembledBytes", &FRenderBatchStats::AssembledBytes),
	                                   Member("uploadBytes", &FRenderBatchStats::UploadBytes),
	                                   Member("gpuReuses", &FRenderBatchStats::GpuReuses),
	                                   Member("compatibilityReuses", &FRenderBatchStats::CompatibilityReuses),
	                                   Member("compatibilityBuilds", &FRenderBatchStats::CompatibilityBuilds),
	                                   Member("instanceContractBuilds", &FRenderBatchStats::InstanceContractBuilds),
	                                   Member("preparedInputBuilds", &FRenderBatchStats::PreparedInputBuilds),
	                                   Member("preparedInputReuses", &FRenderBatchStats::PreparedInputReuses),
	                                   Member("planReuses", &FRenderBatchStats::PlanReuses),
	                                   Member("packetReuses", &FRenderBatchStats::PacketReuses),
	                                   Member("localPlanReuses", &FRenderBatchStats::LocalPlanReuses),
	                                   Member("localPacketReuses", &FRenderBatchStats::LocalPacketReuses),
	                                   Member("localInputReuses", &FRenderBatchStats::LocalInputReuses),
	                                   Member("localCompatibilityReuses", &FRenderBatchStats::LocalCompatibilityReuses),
	                                   Member("localRecordReuses", &FRenderBatchStats::LocalRecordReuses),
	                                   Member("incrementalPlanUpdates", &FRenderBatchStats::IncrementalPlanUpdates),
	                                   Member("incrementalItemReuses", &FRenderBatchStats::IncrementalItemReuses),
	                                   Member("affectedBatches", &FRenderBatchStats::AffectedBatches),
	                                   Member("retainedBatches", &FRenderBatchStats::RetainedBatches),
	                                   Member("batchAdmissionReuses", &FRenderBatchStats::BatchAdmissionReuses),
	                                   Member("evictions", &FRenderBatchStats::Evictions),
	                                   Member("cachedPlanItems", &FRenderBatchStats::CachedPlanItems),
	                                   Member("cachedPlanBlocks", &FRenderBatchStats::CachedPlanBlocks),
	                                   Member("cachedInputs", &FRenderBatchStats::CachedInputs),
	                                   Member("cachedInputBytes", &FRenderBatchStats::CachedInputBytes),
	                                   Member("cachedChunks", &FRenderBatchStats::CachedChunks),
	                                   Member("cachedBytes", &FRenderBatchStats::CachedBytes),
	                                   Member("planningMilliseconds", &FRenderBatchStats::PlanningMilliseconds),
	                                   Member("preparationMilliseconds", &FRenderBatchStats::PreparationMilliseconds),
	                                   Member("fallbacks", &FRenderBatchStats::Fallbacks)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneVisibilityStats>()
{
	static const auto Type = MakeRecord<FSceneVisibilityStats>(
	    "hyperion.diagnostics.scenevisibilitystats",
	    {Member("groups", &FSceneVisibilityStats::Groups),
	     Member("unboundedGroups", &FSceneVisibilityStats::UnboundedGroups),
	     Member("primitives", &FSceneVisibilityStats::Primitives),
	     Member("visitedNodes", &FSceneVisibilityStats::VisitedNodes),
	     Member("groupTests", &FSceneVisibilityStats::GroupTests),
	     Member("candidateGroups", &FSceneVisibilityStats::CandidateGroups),
	     Member("candidatePrimitives", &FSceneVisibilityStats::CandidatePrimitives),
	     Member("collectedPrimitives", &FSceneVisibilityStats::CollectedPrimitives),
	     Member("emittedItems", &FSceneVisibilityStats::EmittedItems),
	     Member("visibleItems", &FSceneVisibilityStats::VisibleItems),
	     Member("draws", &FSceneVisibilityStats::Draws),
	     Member("indexRebuilds", &FSceneVisibilityStats::IndexRebuilds),
	     Member("indexRefits", &FSceneVisibilityStats::IndexRefits),
	     Member("membershipReuses", &FSceneVisibilityStats::MembershipReuses),
	     Member("membershipAdded", &FSceneVisibilityStats::MembershipAdded),
	     Member("membershipRemoved", &FSceneVisibilityStats::MembershipRemoved),
	     Member("containedItemTests", &FSceneVisibilityStats::ContainedItemTests),
	     Member("collectionReuses", &FSceneVisibilityStats::CollectionReuses),
	     Member("preparationReuses", &FSceneVisibilityStats::PreparationReuses),
	     Member("packetReuses", &FSceneVisibilityStats::PacketReuses),
	     Member("itemPreparationReuses", &FSceneVisibilityStats::ItemPreparationReuses),
	     Member("itemStorageReuses", &FSceneVisibilityStats::ItemStorageReuses),
	     Member("sharedMaterialUpdates", &FSceneVisibilityStats::SharedMaterialUpdates),
	     Member("sharedMaterialGroups", &FSceneVisibilityStats::SharedMaterialGroups),
	     Member("retainedMaterialItems", &FSceneVisibilityStats::RetainedMaterialItems),
	     Member("retainedSceneItems", &FSceneVisibilityStats::RetainedSceneItems),
	     Member("retainedItemRestores", &FSceneVisibilityStats::RetainedItemRestores),
	     Member("updateMilliseconds", &FSceneVisibilityStats::UpdateMilliseconds),
	     Member("queryMilliseconds", &FSceneVisibilityStats::QueryMilliseconds),
	     Member("materialMilliseconds", &FSceneVisibilityStats::MaterialMilliseconds),
	     Member("batches", &FSceneVisibilityStats::Batches)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FLocalLightStatistics>()
{
	static const auto Type = MakeRecord<FLocalLightStatistics>(
	    "hyperion.diagnostics.locallightstatistics",
	    {Member("points", &FLocalLightStatistics::Points), Member("spots", &FLocalLightStatistics::Spots),
	     Member("visiblePoints", &FLocalLightStatistics::VisiblePoints),
	     Member("visibleSpots", &FLocalLightStatistics::VisibleSpots), Member("draws", &FLocalLightStatistics::Draws),
	     Member("active", &FLocalLightStatistics::bActive), Member("clustered", &FLocalLightStatistics::bClustered),
	     Member("clusterCells", &FLocalLightStatistics::ClusterCells),
	     Member("clusterOccupied", &FLocalLightStatistics::ClusterOccupied),
	     Member("clusterReferences", &FLocalLightStatistics::ClusterReferences),
	     Member("clusterMaximum", &FLocalLightStatistics::ClusterMaximum),
	     Member("clusterBytes", &FLocalLightStatistics::ClusterBytes),
	     Member("clusterBuildMilliseconds", &FLocalLightStatistics::ClusterBuildMilliseconds),
	     Member("clusterRebuilt", &FLocalLightStatistics::bClusterRebuilt),
	     Member("spatial", &FLocalLightStatistics::Spatial)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSelectionOutlineStatistics>()
{
	static const auto Type = MakeRecord<FSelectionOutlineStatistics>(
	    "hyperion.diagnostics.selectionoutlinestatistics",
	    {Member("objects", &FSelectionOutlineStatistics::Objects),
	     Member("maskPasses", &FSelectionOutlineStatistics::MaskPasses),
	     Member("items", &FSelectionOutlineStatistics::Items),
	     Member("pendingItems", &FSelectionOutlineStatistics::PendingItems),
	     Member("unsupportedItems", &FSelectionOutlineStatistics::UnsupportedItems),
	     Member("rejectedPublication", &FSelectionOutlineStatistics::bRejectedPublication)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FHierarchicalDepthStats>()
{
	static const auto Type = MakeRecord<FHierarchicalDepthStats>(
	    "hyperion.diagnostics.hierarchicaldepthstats",
	    {Member("consumers", &FHierarchicalDepthStats::Consumers),
	     Member("products", &FHierarchicalDepthStats::Products),
	     Member("dispatches", &FHierarchicalDepthStats::Dispatches), Member("bytes", &FHierarchicalDepthStats::Bytes)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FRenderViewStatistics>()
{
	static const auto Type = MakeRecord<FRenderViewStatistics>(
	    "hyperion.diagnostics.renderviewstatistics",
	    {Member("identity", &FRenderViewStatistics::Identity), Member("usage", &FRenderViewStatistics::Usage),
	     Member("visibility", &FRenderViewStatistics::Visibility)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FForwardPipelineStatistics>()
{
	static const auto Type = MakeRecord<FForwardPipelineStatistics>(
	    "hyperion.diagnostics.forwardpipelinestatistics",
	    {Member("preparationMilliseconds", &FForwardPipelineStatistics::PreparationMilliseconds),
	     Member("shadowSetupMilliseconds", &FForwardPipelineStatistics::ShadowSetupMilliseconds),
	     Member("fullscreenPreparationMilliseconds", &FForwardPipelineStatistics::FullscreenPreparationMilliseconds),
	     Member("sceneTargetBytes", &FForwardPipelineStatistics::SceneTargetBytes),
	     Member("fullscreenDraws", &FForwardPipelineStatistics::FullscreenDraws),
	     Member("spatial", &FForwardPipelineStatistics::Spatial),
	     Member("localLights", &FForwardPipelineStatistics::LocalLights),
	     Member("selectionOutline", &FForwardPipelineStatistics::SelectionOutline),
	     Member("views", &FForwardPipelineStatistics::Views),
	     Member("shadowTextureBytes", &FForwardPipelineStatistics::ShadowTextureBytes),
	     Member("shadows", &FForwardPipelineStatistics::bShadows),
	     Member("contactShadows", &FForwardPipelineStatistics::bContactShadows),
	     Member("hierarchicalDepth", &FForwardPipelineStatistics::HierarchicalDepth),
	     Member("cameraStatus", &FForwardPipelineStatistics::CameraStatus)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FRenderDiagnostics>()
{
	static const auto Type = MakeRecord<FRenderDiagnostics>(
	    "hyperion.render.diagnostics",
	    {Member("frame", &FRenderDiagnostics::Frame), Member("ready", &FRenderDiagnostics::bReady),
	     Member("sceneError", &FRenderDiagnostics::SceneError), Member("pipeline", &FRenderDiagnostics::Pipeline),
	     Member("adapter", &FRenderDiagnostics::Adapter), Member("device", &FRenderDiagnostics::Device),
	     Member("gpuPassMilliseconds", &FRenderDiagnostics::GpuPassMilliseconds),
	     Member("gpuTimingFrame", &FRenderDiagnostics::GpuTimingFrame)});
	return Type;
}

void SetDeviceDiagnostics(FRenderDiagnostics& InResult, const FDeviceStats& InDevice)
{
	InResult.Adapter = InDevice.Adapter;
	InResult.Device = {{"validationErrors", InDevice.ValidationErrors},
	                   {"submittedFrames", InDevice.SubmittedFrames},
	                   {"gpuAllocationBytes", InDevice.GpuAllocationBytes},
	                   {"descriptorAllocations", InDevice.DescriptorAllocations},
	                   {"descriptorCopies", InDevice.DescriptorCopies},
	                   {"bindingSetsCreated", InDevice.BindingSetsCreated},
	                   {"graphicsRootBinds", InDevice.GraphicsRootBinds},
	                   {"graphicsHeapBinds", InDevice.GraphicsHeapBinds},
	                   {"graphicsConstantBinds", InDevice.GraphicsConstantBinds},
	                   {"graphicsTableBinds", InDevice.GraphicsTableBinds},
	                   {"pipelinesCreated", InDevice.PipelinesCreated},
	                   {"constantBytesWritten", InDevice.ConstantBytesWritten},
	                   {"graphicsPipelineBinds", InDevice.GraphicsPipelineBinds},
	                   {"graphicsGeometryBinds", InDevice.GraphicsGeometryBinds},
	                   {"graphicsDynamicBinds", InDevice.GraphicsDynamicBinds},
	                   {"commandListsCreated", InDevice.CommandListsCreated},
	                   {"commandListResets", InDevice.CommandListResets}};
	InResult.GpuTimingFrame = InDevice.GpuTiming.Frame;
	for (const auto& Pass : InDevice.GpuTiming.Passes)
	{
		InResult.GpuPassMilliseconds[Pass.Name] = Pass.Milliseconds;
	}
}
} // namespace Hyperion
