#pragma once
#include "Hyperion/RenderControls/RenderBatchStats.h"
#include "Hyperion/RenderControls/ScenePublicationToken.h"
#include <optional>
#include <string>
#include <vector>

namespace Hyperion
{
struct FSceneVisibilityStats
{
	std::size_t Groups{};
	std::size_t UnboundedGroups{};
	std::size_t Primitives{};
	std::size_t VisitedNodes{};
	std::size_t GroupTests{};
	std::size_t CandidateGroups{};
	std::size_t CandidatePrimitives{};
	std::size_t CollectedPrimitives{};
	std::size_t EmittedItems{};
	std::size_t VisibleItems{};
	std::size_t Draws{};
	std::size_t IndexRebuilds{};
	std::size_t IndexRefits{};
	std::size_t MembershipReuses{};
	std::size_t MembershipAdded{};
	std::size_t MembershipRemoved{};
	std::size_t ContainedItemTests{};
	std::size_t CollectionReuses{};
	std::size_t PreparationReuses{};
	std::size_t PacketReuses{};
	std::size_t ItemPreparationReuses{};
	std::size_t ItemStorageReuses{};
	std::size_t SharedMaterialUpdates{};
	std::size_t SharedMaterialGroups{};
	std::size_t RetainedMaterialItems{};
	std::size_t RetainedSceneItems{};
	std::size_t RetainedItemRestores{};
	double UpdateMilliseconds{};
	double QueryMilliseconds{};
	double MaterialMilliseconds{};
	FRenderBatchStats Batches;
};

struct FLocalLightStatistics
{
	std::size_t Points{};
	std::size_t Spots{};
	std::size_t VisiblePoints{};
	std::size_t VisibleSpots{};
	std::size_t Draws{};
	bool bActive{};
	bool bClustered{};
	std::size_t ClusterCells{};
	std::size_t ClusterOccupied{};
	std::size_t ClusterReferences{};
	std::size_t ClusterMaximum{};
	std::size_t ClusterBytes{};
	double ClusterBuildMilliseconds{};
	bool bClusterRebuilt{};
	FSceneVisibilityStats Spatial;
};

struct FHierarchicalDepthStats
{
	std::uint32_t Consumers{};
	std::uint32_t Products{};
	std::uint32_t Dispatches{};
	std::uint64_t Bytes{};
};

enum class ERenderViewStatsCategory
{
	Main,
	Shadow,
	Uncounted
};

enum class ESceneCameraStatus : std::uint8_t
{
	Active,
	DefaultFallback,
	NoActiveCamera,
	EmptyViewport
};

struct FRenderViewStatistics
{
	std::uint64_t Identity{};
	std::string Usage;
	FSceneVisibilityStats Visibility;
	ERenderViewStatsCategory StatsCategory =
	    ERenderViewStatsCategory::Main; // Runtime-only; not part of diagnostic wire.
};

struct FRenderViewFamilyStatistics
{
	std::vector<FRenderViewStatistics> Views;
	FSceneVisibilityStats Spatial;
	double Milliseconds{};
};

struct FSelectionOutlineStatistics
{
	std::size_t Objects{};
	std::size_t MaskPasses{};
	std::size_t Items{};
	std::size_t PendingItems{};
	std::size_t UnsupportedItems{};
	bool bRejectedPublication{};
};

struct FForwardPipelineStatistics
{
	double PreparationMilliseconds{};
	double ShadowSetupMilliseconds{};
	double FullscreenPreparationMilliseconds{};
	std::uint64_t SceneTargetBytes{};
	std::size_t FullscreenDraws{};
	FSceneVisibilityStats Spatial;
	FLocalLightStatistics LocalLights;
	FSelectionOutlineStatistics SelectionOutline;
	std::vector<FRenderViewStatistics> Views;
	std::uint64_t ShadowTextureBytes{};
	bool bShadows{};
	bool bContactShadows{};
	FHierarchicalDepthStats HierarchicalDepth;
	std::optional<FScenePublicationToken> SceneToken;
	ESceneCameraStatus CameraStatus = ESceneCameraStatus::Active;
	FSceneVisibilityStats MainView() const;
};
} // namespace Hyperion
