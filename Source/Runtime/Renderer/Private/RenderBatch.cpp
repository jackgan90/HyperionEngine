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
	auto Result = std::make_shared<FRenderBatchPlan>();
	const auto Depth = BatchDepth(InSnapshot);
	std::multimap<std::size_t, FBatchGroup> Groups;
	for (std::size_t Index = 0; Index < InSnapshot.Items.size(); ++Index)
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
				Candidate = P.Describe(InSnapshot, Item, {bSrgb, Depth}, Result->Statistics);
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
	Result->Statistics.CachedChunks = P.Chunks.size();
	Result->Statistics.CachedBytes = P.ChunkBytes;
	Result->Statistics.PlanningMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	return Result;
}

void FRenderBatchSystem::Clear()
{
	Impl->Tasks.Require({EDomain::Render});
	Impl->Items.clear();
	Impl->RecentItems.clear();
	Impl->Chunks.clear();
	Impl->RecentChunks.clear();
	Impl->ChunkBytes = 0;
	Impl->Plans.clear();
	Impl->PlanItems = 0;
}
} // namespace Hyperion
