#include "RenderResourcesInternal.h"
#include <algorithm>

namespace Hyperion
{
bool FRenderResourceCoordinator::ReuseViewPasses(const FRenderSceneSnapshot& InSnapshot,
                                                 std::vector<FColorPass>& OutPasses)
{
	if (!InSnapshot.ContentIdentity || !InSnapshot.Frame)
	{
		return false;
	}
	const FViewKey Key{InSnapshot.Frame->Session, InSnapshot.Family, InSnapshot.View.Identity};
	const auto It = PreparedViews.find(Key);
	if (It == PreparedViews.end() || It->second.Contents.lock() != InSnapshot.ContentIdentity ||
	    It->second.ResourceRevision != PublicationRevision.load(std::memory_order_acquire))
	{
		return false;
	}
	OutPasses = It->second.Passes;
	if (InSnapshot.View.Name.empty())
	{
		for (std::size_t Index = 0; Index < OutPasses.size(); ++Index)
		{
			OutPasses[Index].Commands.Name =
			    "Scene " + std::to_string(InSnapshot.Frame->Session) + "/" + std::to_string(InSnapshot.Frame->Frame) +
			    "/" + std::to_string(InSnapshot.Family) + "/" + std::to_string(InSnapshot.View.Identity) + "/" +
			    InSnapshot.View.Usage + "/" + std::to_string(Index);
		}
	}
	Stats.Batches = It->second.Statistics;
	Stats.Batches.ReusedChunks = Stats.Batches.InstancedDraws;
	Stats.Batches.RebuiltChunks = 0;
	Stats.Batches.PackedBytes = 0;
	Stats.Batches.UploadBytes = 0;
	Stats.Batches.GpuReuses = 0;
	Stats.Batches.CompatibilityReuses = 0;
	Stats.Batches.CompatibilityBuilds = 0;
	Stats.Batches.PlanReuses = 1;
	Stats.Batches.PacketReuses = 1;
	Stats.Batches.Evictions = 0;
	Stats.Batches.PlanningMilliseconds = 0;
	Stats.Batches.PreparationMilliseconds = 0;
	// Reused packets still publish this view's receipts in the current family order.
	// Cache entries are published only after every item's preparation succeeds.
	for (const auto& Item : InSnapshot.Items)
	{
		if (Item.Report)
		{
			Item.Report({InSnapshot.Frame->Frame, InSnapshot.Family, InSnapshot.View.Identity, Item.State.Revision,
			             Item.State.Surface ? Item.State.Surface->GetSnapshot()->Revision : 0, InSnapshot.View.Usage,
			             true, ""});
		}
	}
	return true;
}

void FRenderResourceCoordinator::CacheViewPasses(const FRenderSceneSnapshot& InSnapshot,
                                                 std::vector<FColorPass>& InPasses)
{
	if (!InSnapshot.ContentIdentity || !InSnapshot.Frame || Stats.Batches.FailedItems)
	{
		return;
	}
	const FViewKey Key{InSnapshot.Frame->Session, InSnapshot.Family, InSnapshot.View.Identity};
	if (!PreparedViews.contains(Key) && PreparedViews.size() >= 64)
	{
		CollectViewPasses();
		if (PreparedViews.size() >= 64)
		{
			return;
		}
	}
	for (auto& Pass : InPasses)
	{
		Pass.Commands.ShareDraws();
	}
	TrackScope(InSnapshot.ContentIdentity);
	PreparedViews.insert_or_assign(Key, FPreparedViewPasses{InSnapshot.ContentIdentity,
	                                                        PublicationRevision.load(std::memory_order_acquire),
	                                                        InPasses, Stats.Batches});
}

void FRenderResourceCoordinator::CollectViewPasses()
{
	std::erase_if(PreparedViews,
	              [](const auto& InEntry)
	              {
		              return InEntry.second.Contents.expired();
	              });
}
} // namespace Hyperion
