#include "SceneInternal.h"
#include "SceneQueryInternal.h"
#include <algorithm>
#include <cmath>

namespace Hyperion
{
namespace
{
void SynchronizeQueries(FSceneStorage& InStorage, FBoundsQueryStats& OutStats)
{
	if (!InStorage.QueryIndex)
	{
		auto Index = CreateBoundsBvh();
		std::set<std::uint32_t> Dirty(InStorage.Order.begin(), InStorage.Order.end());
		InStorage.QueryIndex = std::move(Index);
		InStorage.QueryDirty = std::move(Dirty);
	}
	for (const auto Slot : InStorage.QueryDirty)
	{
		const auto& Entry = *InStorage.Slots.at(Slot);
		if (Entry.Node && Entry.Node->Model() && Entry.Transfer.bVisible)
		{
			InStorage.QueryIndex->Set(Slot, TransformBounds(SceneModelBounds(Entry.Transfer), Entry.World));
		}
		else
		{
			InStorage.QueryIndex->Remove(Slot);
		}
	}
	InStorage.QueryIndex->Commit(OutStats);
	InStorage.QueryDirty.clear();
}
} // namespace

float SceneRayMaximum(const FSceneRayResult& InResult, float InMaximum)
{
	if (InResult.Status != ESceneRayStatus::Hit)
	{
		return InMaximum;
	}
	// The reported float may round below the double intersection. Keep ties reachable,
	// but never widen the caller's clipping interval or an already tighter limit.
	return std::min(InMaximum, std::nextafter(InResult.Distance, std::numeric_limits<float>::infinity()));
}

FSceneRayResult FScene::Raycast(FRay InRay, const FSceneRayOptions& InOptions)
{
	RequireMain();
	FSceneRayResult Result;
	if (!IsUsable(InRay))
	{
		return Result;
	}
	const float DirectionLength = Length(InRay.Direction);
	if (!std::isfinite(DirectionLength) || DirectionLength <= 0)
	{
		return Result;
	}
	InRay.Direction = ScaleVector(InRay.Direction, 1 / DirectionLength);
	SynchronizeQueries(*Storage, Result.Stats.Bounds);
	Result.Status = ESceneRayStatus::Miss;
	Result.Distance = InRay.Maximum;
	Storage->QueryIndex->Raycast(
	    InRay,
	    [&](std::uint64_t InSlot, float& OutMaximum)
	    {
		    const auto Slot = static_cast<std::uint32_t>(InSlot);
		    const auto& Entry = *Storage->Slots.at(Slot);
		    FRay Ray = InRay;
		    Ray.Maximum = OutMaximum;
		    RaycastSceneModel(Entry.Transfer, Storage->Handle(Slot), Ray, InOptions, Result);
		    if (Result.Status == ESceneRayStatus::Hit)
		    {
			    OutMaximum = SceneRayMaximum(Result, OutMaximum);
		    }
	    },
	    Result.Stats.Bounds);
	Result.bIncomplete = Result.Stats.UnavailableCandidates != 0;
	if (Result.Status == ESceneRayStatus::Miss && Result.bIncomplete)
	{
		Result.Status = ESceneRayStatus::Unavailable;
	}
	return Result;
}
} // namespace Hyperion
