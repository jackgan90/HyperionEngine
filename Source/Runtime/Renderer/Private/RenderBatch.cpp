#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderResources.h"
#include "RenderBatchInternal.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace Hyperion
{
namespace
{
struct FBatchGroup
{
	std::shared_ptr<const FRenderBatchCandidate> Candidate;
	const IRenderBatchStrategy* Strategy{};
	std::uint32_t Capacity{};
	std::size_t Batch{};
};

ERHIDepthFormat BatchDepth(const FRenderSceneSnapshot& InSnapshot)
{
	bool bDepth = false;
	for (const auto& Item : InSnapshot.Items)
	{
		if (Item.PreparationError.empty() && Item.State.Surface &&
		    Item.State.Surface->GetSnapshot()->Definition->HasPass(InSnapshot.View.Usage))
		{
			const auto& State = Item.State.Surface->GetSnapshot()->Definition->GetPass(InSnapshot.View.Usage).State;
			bDepth |= State.bDepthTest || State.bStencil;
		}
	}
	return bDepth ? InSnapshot.DepthFormat : ERHIDepthFormat::None;
}

void AppendGroup(FRenderBatchPlan& InPlan, std::multimap<std::size_t, FBatchGroup>& InGroups,
                 std::shared_ptr<const FRenderBatchCandidate> InCandidate, const IRenderBatchStrategy* InStrategy,
                 std::uint32_t InCapacity, std::size_t InItem)
{
	HYP_PERF_SCOPE_C(Detail, GroupBatchItem);
	const auto Hash = InCandidate->CompatibilityHash;
	auto& Singletons = InPlan.Statistics.Fallbacks[static_cast<std::size_t>(ERenderBatchFallback::Singleton)];
	const auto [Begin, End] = InGroups.equal_range(Hash);
	for (auto It = Begin; It != End; ++It)
	{
		auto& Group = It->second;
		if (Group.Strategy == InStrategy && InStrategy->CanCombine(*Group.Candidate, *InCandidate))
		{
			if (InPlan.Batches[Group.Batch].Items.size() >= std::min(Group.Capacity, InCapacity))
			{
				++InPlan.Statistics.CapacitySplits;
				Group.Batch = InPlan.Batches.size();
				InPlan.Batches.push_back({});
				++Singletons;
			}
			else if (InPlan.Batches[Group.Batch].Items.size() == 1)
			{
				--Singletons;
			}
			Group.Capacity = std::min(Group.Capacity, InCapacity);
			InPlan.Batches[Group.Batch].Items.push_back(InItem);
			return;
		}
	}
	InGroups.emplace(Hash, FBatchGroup{std::move(InCandidate), InStrategy, InCapacity, InPlan.Batches.size()});
	InPlan.Batches.push_back({{InItem}});
	++Singletons;
}
} // namespace

FRenderBatchSystem::FRenderBatchSystem(FTaskSystem& InTasks, FRHICapabilities InCapabilities,
                                       FRenderBatchLimits InLimits)
    : Impl(std::make_unique<FImpl>(InTasks, std::move(InCapabilities), InLimits))
{
}

FRenderBatchSystem::~FRenderBatchSystem() = default;

void FRenderBatchSystem::InvalidatePlans()
{
	Impl->Tasks.Require({EDomain::Render});
	Impl->Plans.clear();
	Impl->PlanItems = 0;
}

bool FRenderBatchSystem::HasCustomStrategies() const
{
	Impl->Tasks.Require({EDomain::Render});
	return Impl->Strategies.size() != 1;
}

void FRenderBatchSystem::Register(std::unique_ptr<IRenderBatchStrategy> InStrategy)
{
	Impl->Tasks.Require({EDomain::Main});
	if (!InStrategy || Impl->bStarted)
	{
		throw std::logic_error("Register batch strategies before the first build");
	}
	Impl->Strategies.insert(Impl->Strategies.begin(), std::move(InStrategy));
}

std::shared_ptr<FRenderBatchPlan> FRenderBatchSystem::FImpl::BuildFresh(const FRenderSceneSnapshot& InSnapshot,
                                                                        bool bInEnabled)
{
	auto& P = *this;
	HYP_PERF_SCOPE_C(Detail, BuildFreshBatchPlan);
	auto Result = std::make_shared<FRenderBatchPlan>();
	if (bInEnabled)
	{
		PrepareInputs(InSnapshot, Result->Statistics);
	}
	const auto Depth = BatchDepth(InSnapshot);
	std::multimap<std::size_t, FBatchGroup> Groups;
	FSharedBatchValueCache Shared;
	for (std::size_t Index = 0; Index < InSnapshot.Items.Size(); ++Index)
	{
		const auto& Item = InSnapshot.Items[Index];
		ERenderBatchFallback Reason = bInEnabled ? ERenderBatchFallback::Preparation : ERenderBatchFallback::Disabled;
		const IRenderBatchStrategy* Selected{};
		std::shared_ptr<const FRenderBatchCandidate> Candidate;
		std::uint32_t Capacity{};
		if (bInEnabled && Item.PreparationError.empty() && Item.State.Surface && Item.State.Resource &&
		    Item.ResolvedParameters)
		{
			try
			{
				const bool bSrgb =
				    Item.State.Surface->GetSnapshot()->Definition->GetPass(InSnapshot.View.Usage).bSrgbTarget;
				Candidate = P.Describe(InSnapshot, Item, {bSrgb, Depth, InSnapshot.View.DepthTarget ? 0U : 1U},
				                       Result->Statistics, Shared);
				for (const auto& Strategy : P.Strategies)
				{
					const auto Decision = Strategy->Evaluate(*Candidate, P.Capabilities);
					Reason = Decision.Reason;
					if (Decision.Capacity >= 2)
					{
						Selected = Strategy.get();
						Capacity = Decision.Capacity;
						break;
					}
				}
			}
			catch (const std::exception&)
			{
				Reason = ERenderBatchFallback::Preparation;
			}
		}
		if (Selected)
		{
			++Result->Statistics.EligibleItems;
			AppendGroup(*Result, Groups, std::move(Candidate), Selected, Capacity, Index);
		}
		else
		{
			Groups.clear(); // Unclaimed/order-sensitive items are barriers, including future strategies' fallback.
			++Result->Statistics.Fallbacks[static_cast<std::size_t>(Reason)];
			Result->Batches.push_back({{Index}});
		}
	}
	for (auto& Batch : Result->Batches)
	{
		if (Batch.Items.size() > 1)
		{
			try
			{
				Batch.Instances = P.Data(InSnapshot, Batch, Result->Statistics);
			}
			catch (const std::exception&)
			{
				Result->Statistics.Fallbacks[static_cast<std::size_t>(ERenderBatchFallback::Preparation)] +=
				    Batch.Items.size();
			}
		}
	}
	return Result;
}

std::shared_ptr<const FRenderBatchPlan> FRenderBatchSystem::Build(const FRenderSceneSnapshot& InSnapshot,
                                                                  bool bInEnabled)
{
	HYP_PERF_SCOPE_C(Render, PlanBatches);
	auto& P = *Impl;
	P.Tasks.Require({EDomain::Render});
	P.bStarted = true;
	++P.Access;
	const auto Start = std::chrono::steady_clock::now();
	std::shared_ptr<FRenderBatchPlan> Result;
	if (bInEnabled)
	{
		Result = P.ReusePlan(InSnapshot);
	}
	if (!Result || !bInEnabled)
	{
		Result = P.BuildFresh(InSnapshot, bInEnabled);
		if (bInEnabled)
		{
			P.CachePlan(InSnapshot, *Result);
		}
	}
	P.Retire(InSnapshot);
	P.CurrentInputs.clear();
	if (P.CurrentInputs.capacity() > P.Limits.MaxItems)
	{
		P.CurrentInputs.shrink_to_fit();
	}
	HYP_PERF_PLOT(Render, BatchPlanReuses, double(Result->Statistics.PlanReuses));
	HYP_PERF_PLOT(Render, BatchPreparedInputBuilds, double(Result->Statistics.PreparedInputBuilds));
	HYP_PERF_PLOT(Render, BatchPreparedInputReuses, double(Result->Statistics.PreparedInputReuses));
	HYP_PERF_PLOT(Render, BatchInstanceContractBuilds, double(Result->Statistics.InstanceContractBuilds));
	Result->Statistics.CachedInputs = P.Prepared.size();
	Result->Statistics.CachedInputBytes = P.PreparedMetadataBytes;
	Result->Statistics.CachedChunks = P.Chunks.size();
	Result->Statistics.CachedBytes = P.ChunkBytes + P.Packing.ByteSize();
	HYP_PERF_PLOT(Render, BatchCachedInputs, double(Result->Statistics.CachedInputs));
	HYP_PERF_PLOT(Render, BatchCachedInputBytes, double(Result->Statistics.CachedInputBytes));
	Result->Statistics.PlanningMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	return Result;
}

void FRenderBatchSystem::Clear()
{
	Impl->Tasks.Require({EDomain::Render});
	Impl->Items.clear();
	Impl->Structures.clear();
	Impl->RecentItems.clear();
	Impl->Chunks.clear();
	Impl->RecentChunks.clear();
	Impl->Contracts.clear();
	Impl->Prepared.clear();
	Impl->RecentPrepared.clear();
	Impl->CurrentInputs.clear();
	Impl->PreparedMetadataBytes = 0;
	Impl->PreparedCursor.reset();
	Impl->ChunkBytes = 0;
	Impl->Plans.clear();
	Impl->PlanItems = 0;
	Impl->RetiredFamily = {};
	Impl->Packing.Clear();
}
} // namespace Hyperion
