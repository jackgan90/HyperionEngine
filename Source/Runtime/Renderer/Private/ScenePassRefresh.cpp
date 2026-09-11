#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderBatch.h"
#include "MaterialProfiling.h"
#include "RenderResourcesInternal.h"
#include <chrono>

namespace Hyperion
{
namespace
{
void RefreshDrawPackets(FRenderResourceCoordinator& InOwner, const FRenderSceneSnapshot& InSnapshot,
                        const FRenderResourceCoordinator::FPreparedViewPasses& InCached,
                        std::vector<FGraphicsDrawBatch>& OutPasses)
{
	OutPasses = InCached.Passes;
	std::size_t Index{};
	for (auto& Pass : OutPasses)
	{
		Pass.Commands.MaterializeDraws();
		for (auto& Draw : Pass.Commands.Draws)
		{
			const auto& Source = InCached.Sources.at(Index++);
			Draw = InOwner.DrawMaterial(InSnapshot.Items.At(Source.Item), InSnapshot.View,
			                            InSnapshot.Targets.GraphicsTarget(Pass.bSrgb), Source.Batch.has_value());
			if (Source.Batch)
			{
				const auto& Data = InSnapshot.Batches->Batches.at(*Source.Batch).Instances;
				if (!Data || !Data->IsLive())
				{
					throw std::invalid_argument("Retired instance data in a retained pass");
				}
				InOwner.BindInstances(Draw, Data);
			}
		}
		Pass.Commands.ShareDraws();
	}
	if (Index != InCached.Sources.size())
	{
		throw std::invalid_argument("Retained pass source count mismatch");
	}
}
} // namespace

bool FRenderResourceCoordinator::RefreshViewPasses(const FRenderSceneSnapshot& InSnapshot,
                                                   FPreparedViewPasses& InCached,
                                                   std::vector<FGraphicsDrawBatch>& OutPasses)
{
	if (!InSnapshot.LocalContentIdentity || InCached.LocalContents.lock() != InSnapshot.LocalContentIdentity ||
	    !InSnapshot.Batches || !InSnapshot.Batches->StructureIdentity ||
	    InCached.BatchStructure.lock() != InSnapshot.Batches->StructureIdentity ||
	    InCached.Statistics.Fallbacks[static_cast<std::size_t>(ERenderBatchFallback::Preparation)] != 0)
	{
		return false;
	}
	HYP_PERF_SCOPE_C(Detail, RefreshRetainedScenePasses);
	const auto Start = std::chrono::steady_clock::now();
	const auto Before = MaterialConstants->Statistics();
#if HYP_ENABLE_PROFILING
	FRenderResourceStats ProfileBefore;
	const bool bProfile = IsProfilingEnabled(EProfileCategory::Material);
	if (bProfile)
	{
		ProfileBefore.Constants = Before;
		ProfileBefore.Materials = MaterialGpu->Statistics();
	}
#endif
	try
	{
		RefreshDrawPackets(*this, InSnapshot, InCached, OutPasses);
	}
	catch (const std::exception&)
	{
		OutPasses.clear();
		return false; // Publish failures through the ordinary per-source validation and repair transaction.
	}
	auto Updated = InSnapshot.Batches->Statistics;
	Updated.InstancedItems = InCached.Statistics.InstancedItems;
	Updated.InstancedDraws = InCached.Statistics.InstancedDraws;
	Updated.SingleDraws = InCached.Statistics.SingleDraws;
	Updated.LocalPacketReuses = 1;
	const auto After = MaterialConstants->Statistics();
	Updated.UploadBytes = After.InstanceUploadBytes - Before.InstanceUploadBytes;
	Updated.GpuReuses = After.InstanceReuses - Before.InstanceReuses;
	Updated.PreparationMilliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
	// All packets have new immutable ownership before the old view cache is replaced.
	InCached.Passes = OutPasses;
	InCached.Contents = InSnapshot.ContentIdentity;
	InCached.Statistics = Updated;
	Stats.Batches = Updated;
	Stats.Constants = After;
	Stats.Materials = MaterialGpu->Statistics();
#if HYP_ENABLE_PROFILING
	if (bProfile)
	{
		PlotMaterialDrawCounters(ProfileBefore, Stats);
	}
#endif
	return true;
}
} // namespace Hyperion
