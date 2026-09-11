#include "Hyperion/Core/Profiling.h"
#include "MaterialProfiling.h"
#include "SceneDrawsInternal.h"
#include <chrono>
#include <stdexcept>

namespace Hyperion
{
namespace
{
std::vector<FRenderResourceCoordinator::FPreparedViewDraw> PrepareDrawSources(const FRenderSceneSnapshot& InSnapshot,
                                                                              const FPreparedSceneDraws& InPrepared)
{
	std::vector<FRenderResourceCoordinator::FPreparedViewDraw> Result;
	if (!InSnapshot.LocalContentIdentity || !InPrepared.Failures.empty())
	{
		return Result;
	}
	std::vector<std::optional<std::size_t>> Batches(InSnapshot.Items.Size());
	if (InSnapshot.Batches)
	{
		for (std::size_t Index = 0; Index < InSnapshot.Batches->Batches.size(); ++Index)
		{
			const auto& Batch = InSnapshot.Batches->Batches[Index];
			if (Batch.Instances && Batch.Items.size() > 1)
			{
				Batches[Batch.Items.front()] = Index;
			}
		}
	}
	for (std::size_t Index = 0; Index < InPrepared.Packets.size(); ++Index)
	{
		if (InPrepared.Packets[Index])
		{
			Result.push_back({Index, InPrepared.Packets[Index]->InstanceCount > 1 ? Batches[Index] : std::nullopt});
		}
	}
	return Result;
}

FPreparedSceneDraws PrepareDraws(FRenderResourceCoordinator& InOwner, const FRenderSceneSnapshot& InSnapshot)
{
	HYP_PERF_SCOPE_C(Detail, PrepareSceneDrawPackets);
	FPreparedSceneDraws Result;
	Result.Packets.resize(InSnapshot.Items.Size());
	Result.Srgb.resize(InSnapshot.Items.Size());
	Result.Depth = InSnapshot.Targets.GetDepthFormat();
	if (InSnapshot.Batches)
	{
		PrepareBatchedDraws(InOwner, InSnapshot, Result);
		return Result;
	}
	for (std::size_t Index = 0; Index < InSnapshot.Items.Size(); ++Index)
	{
		const auto& Item = InSnapshot.Items[Index];
		try
		{
			if (!Item.PreparationError.empty())
			{
				throw std::runtime_error(Item.PreparationError);
			}
			if (!Item.State.Surface || !Item.State.Resource)
			{
				throw std::invalid_argument("A draw requires ready geometry and a material selection");
			}
			Result.Srgb[Index] =
			    Item.State.Surface->GetSnapshot()->Definition->GetPass(InSnapshot.View.Usage).bSrgbTarget;
			Result.Packets[Index] =
			    InOwner.DrawMaterial(Item, InSnapshot.View, InSnapshot.Targets.GraphicsTarget(Result.Srgb[Index]));
		}
		catch (const std::exception& Error)
		{
			Result.Failures[{Item.Primitive.Scene, Item.Group}] = Error.what();
		}
	}
	return Result;
}

std::vector<FGraphicsDrawBatch> PublishDraws(const FRenderSceneSnapshot& InSnapshot, FPreparedSceneDraws InPrepared)
{
	HYP_PERF_SCOPE_C(Detail, PublishSceneDrawReceipts);
	std::vector<FGraphicsDrawBatch> Passes;
	for (std::size_t Index = 0; Index < InSnapshot.Items.Size(); ++Index)
	{
		const auto& Item = InSnapshot.Items[Index];
		const auto Failed = InPrepared.Failures.find({Item.Primitive.Scene, Item.Group});
		if (Item.Report)
		{
			Item.Report({InSnapshot.Frame ? InSnapshot.Frame->Frame : 0, InSnapshot.Family, InSnapshot.View.Identity,
			             Item.State.Revision, Item.State.Surface ? Item.State.Surface->GetSnapshot()->Revision : 0,
			             InSnapshot.View.Usage, Failed == InPrepared.Failures.end(),
			             Failed == InPrepared.Failures.end() ? "" : Failed->second});
		}
		if (Failed != InPrepared.Failures.end() || !InPrepared.Packets[Index])
		{
			continue;
		}
		if (Passes.empty() || Passes.back().bSrgb != InPrepared.Srgb[Index])
		{
			Passes.push_back({{}, InPrepared.Srgb[Index]});
		}
		Passes.back().Commands.Draws.push_back(std::move(*InPrepared.Packets[Index]));
	}
	return Passes;
}

void CountPreparedDraws(const FRenderSceneSnapshot& InSnapshot, FPreparedSceneDraws& InPrepared)
{
	auto& Stats = InPrepared.Statistics;
	for (std::size_t Index = 0; Index < InSnapshot.Items.Size(); ++Index)
	{
		const auto& Item = InSnapshot.Items[Index];
		if (InPrepared.Failures.contains({Item.Primitive.Scene, Item.Group}))
		{
			++Stats.FailedItems;
		}
		else if (InPrepared.Packets[Index])
		{
			const auto Count = InPrepared.Packets[Index]->InstanceCount;
			Stats.InstancedDraws += Count > 1;
			Stats.InstancedItems += Count > 1 ? Count : 0;
			Stats.SingleDraws += Count == 1;
		}
	}
}

} // namespace

std::vector<FGraphicsDrawBatch> FRenderResourceService::BuildDraws(const FRenderSceneSnapshot& InSnapshot)
{
	return GetPreparation().BuildDraws(InSnapshot);
}

std::vector<FGraphicsDrawBatch> FRenderResourcePreparation::BuildDraws(const FRenderSceneSnapshot& InSnapshot,
                                                                       FRenderBatchStats* OutStatistics) const
{
	HYP_PERF_SCOPE_C(Rhi, PrepareDraws);
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	if (InSnapshot.DrawFrame)
	{
		const auto Frame = InSnapshot.Frame ? InSnapshot.Frame->Frame : 0;
		InSnapshot.DrawFrame->store(std::max(Frame, InSnapshot.DrawFrame->load(std::memory_order_relaxed)),
		                            std::memory_order_release);
	}
	std::vector<FGraphicsDrawBatch> Passes;
	{
		std::lock_guard Lock(Owner.Mutex);
		if (Owner.bClosed)
		{
			throw std::logic_error("Render resource service is closed");
		}
		if (Owner.ReuseViewPasses(InSnapshot, Passes))
		{
			if (OutStatistics)
			{
				*OutStatistics = Owner.Stats.Batches;
			}
			HYP_PERF_PLOT(Rhi, ViewPacketReuses, 1.0);
			return Passes;
		}
		HYP_PERF_PLOT(Rhi, ViewPacketReuses, 0.0);
#if HYP_ENABLE_PROFILING
		FRenderResourceStats Before;
		const bool bProfile = IsProfilingEnabled(EProfileCategory::Material);
		if (bProfile && Owner.MaterialGpu)
		{
			Before.Constants = Owner.MaterialConstants->Statistics();
			Before.Materials = Owner.MaterialGpu->Statistics();
		}
#endif
		const auto Start = std::chrono::steady_clock::now();
		const auto BeforeInstances =
		    Owner.MaterialConstants ? Owner.MaterialConstants->Statistics() : FMaterialConstantStats{};
		auto Prepared = PrepareDraws(Owner, InSnapshot);
		auto& Stats = Prepared.Statistics;
		CountPreparedDraws(InSnapshot, Prepared);
		Stats.PreparationMilliseconds =
		    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
		if (Owner.MaterialConstants)
		{
			const auto After = Owner.MaterialConstants->Statistics();
			Stats.UploadBytes = After.InstanceUploadBytes - BeforeInstances.InstanceUploadBytes;
			Stats.GpuReuses = After.InstanceReuses - BeforeInstances.InstanceReuses;
		}
		Owner.Stats.Batches = Stats;
		if (OutStatistics)
		{
			*OutStatistics = Stats;
		}
		auto Sources = PrepareDrawSources(InSnapshot, Prepared);
		Passes = PublishDraws(InSnapshot, std::move(Prepared));
		Owner.CacheViewPasses(InSnapshot, Passes, std::move(Sources));
		if (Owner.MaterialGpu)
		{
			Owner.Stats.Materials = Owner.MaterialGpu->Statistics();
			Owner.Stats.Constants = Owner.MaterialConstants->Statistics();
#if HYP_ENABLE_PROFILING
			if (bProfile)
			{
				PlotMaterialDrawCounters(Before, Owner.Stats);
			}
#endif
		}
	}
	return Passes;
}
} // namespace Hyperion
