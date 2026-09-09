#include "Hyperion/Core/Profiling.h"
#include "MaterialProfiling.h"
#include "SceneDrawsInternal.h"
#include <chrono>
#include <stdexcept>

namespace Hyperion
{
namespace
{
FPreparedSceneDraws PrepareDraws(FRenderResourceCoordinator& InOwner, const FRenderSceneSnapshot& InSnapshot)
{
	FPreparedSceneDraws Result;
	Result.bDepth = bool(InSnapshot.View.DepthTarget);
	Result.Packets.resize(InSnapshot.Items.size());
	Result.Srgb.resize(InSnapshot.Items.size());
	for (const auto& Item : InSnapshot.Items)
	{
		if (Item.PreparationError.empty() && Item.State.Surface &&
		    Item.State.Surface->GetSnapshot()->Definition->HasPass(InSnapshot.View.Usage))
		{
			const auto& State = Item.State.Surface->GetSnapshot()->Definition->GetPass(InSnapshot.View.Usage).State;
			Result.bDepth |= State.bDepthTest;
			Result.bStencil |= State.bStencil;
		}
	}
	Result.Depth = Result.bDepth || Result.bStencil ? InSnapshot.DepthFormat : ERHIDepthFormat::None;
	if (InSnapshot.Batches)
	{
		PrepareBatchedDraws(InOwner, InSnapshot, Result);
		return Result;
	}
	for (std::size_t Index = 0; Index < InSnapshot.Items.size(); ++Index)
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
			Result.Packets[Index] = InOwner.DrawMaterial(Item, InSnapshot.View, {Result.Srgb[Index], Result.Depth});
		}
		catch (const std::exception& Error)
		{
			Result.Failures[{Item.Primitive.Scene, Item.Group}] = Error.what();
		}
	}
	return Result;
}

FColorPass NewPass(const FRenderSceneSnapshot& InSnapshot, const FPreparedSceneDraws& InPrepared, std::size_t InIndex,
                   bool bInSrgb)
{
	FColorPass Pass;
	const auto Frame = InSnapshot.Frame;
	Pass.Commands.Name = "Scene " + std::to_string(Frame ? Frame->Session : 0) + "/" +
	                     std::to_string(Frame ? Frame->Frame : 0) + "/" + std::to_string(InSnapshot.Family) + "/" +
	                     std::to_string(InSnapshot.View.Identity) + "/" + InSnapshot.View.Usage + "/" +
	                     std::to_string(InIndex);
	Pass.Commands.bUseDepth = InPrepared.bDepth;
	Pass.Commands.bUseStencil = InPrepared.bStencil;
	Pass.Commands.DepthFormat = InPrepared.Depth;
	Pass.Commands.bClearDepth = InPrepared.bDepth && InIndex == 0;
	Pass.Commands.bClearStencil = InPrepared.bStencil && InIndex == 0;
	Pass.Commands.DepthDomain = InSnapshot.View.Identity;
	Pass.Commands.Viewport = InSnapshot.View.Viewport;
	Pass.Commands.bSrgbTarget = bInSrgb;
	if (InIndex == 0 && InSnapshot.View.ClearColor)
	{
		Pass.Load = EColorLoad::Clear;
		Pass.Commands.ClearColor = *InSnapshot.View.ClearColor;
	}
	if (!InSnapshot.View.Name.empty())
	{
		Pass.Commands.Name = InSnapshot.View.Name + "/" + std::to_string(InIndex);
	}
	return Pass;
}

std::vector<FColorPass> PublishDraws(const FRenderSceneSnapshot& InSnapshot, FPreparedSceneDraws InPrepared)
{
	std::vector<FColorPass> Passes;
	for (std::size_t Index = 0; Index < InSnapshot.Items.size(); ++Index)
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
		if (Passes.empty() || Passes.back().Commands.bSrgbTarget != InPrepared.Srgb[Index])
		{
			Passes.push_back(NewPass(InSnapshot, InPrepared, Passes.size(), InPrepared.Srgb[Index]));
		}
		Passes.back().Commands.Draws.push_back(std::move(*InPrepared.Packets[Index]));
	}
	if (Passes.empty() && (InSnapshot.View.DepthTarget || InSnapshot.View.ClearColor))
	{
		Passes.push_back(NewPass(InSnapshot, InPrepared, 0, false));
	}
	return Passes;
}
} // namespace

std::vector<FColorPass> FRenderResourceService::BuildPasses(const FRenderSceneSnapshot& InSnapshot)
{
	HYP_PERF_SCOPE_C(Rhi, PrepareDraws);
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::vector<FColorPass> Passes;
	{
		std::lock_guard Lock(Owner.Mutex);
		if (Owner.bClosed)
		{
			throw std::logic_error("Render resource service is closed");
		}
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
		for (std::size_t Index = 0; Index < InSnapshot.Items.size(); ++Index)
		{
			const auto& Item = InSnapshot.Items[Index];
			if (Prepared.Failures.contains({Item.Primitive.Scene, Item.Group}))
			{
				++Stats.FailedItems;
			}
			else if (Prepared.Packets[Index])
			{
				const auto Count = Prepared.Packets[Index]->InstanceCount;
				Stats.InstancedDraws += Count > 1;
				Stats.InstancedItems += Count > 1 ? Count : 0;
				Stats.SingleDraws += Count == 1;
			}
		}
		Stats.PreparationMilliseconds =
		    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
		if (Owner.MaterialConstants)
		{
			const auto After = Owner.MaterialConstants->Statistics();
			Stats.UploadBytes = After.InstanceUploadBytes - BeforeInstances.InstanceUploadBytes;
			Stats.GpuReuses = After.InstanceReuses - BeforeInstances.InstanceReuses;
		}
		Owner.Stats.Batches = Stats;
		Passes = PublishDraws(InSnapshot, std::move(Prepared));
		if (InSnapshot.View.DepthTarget || !InSnapshot.View.SampledDepth.empty())
		{
			Owner.EnsureMaterialCaches();
			const FMaterialResourceOwners Owners{InSnapshot.View.TargetLifetime};
			for (auto& Pass : Passes)
			{
				if (InSnapshot.View.DepthTarget)
				{
					Pass.Commands.bUseColor = false;
					Pass.Commands.DepthTarget = Owner.MaterialGpu->GetTexture(InSnapshot.View.DepthTarget, Owners);
				}
				for (const auto& Source : InSnapshot.View.SampledDepth)
				{
					Pass.Commands.SampledDepth.push_back(Owner.MaterialGpu->GetTexture(Source, Owners));
				}
			}
		}
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
	Owner.Schedule();
	return Passes;
}
} // namespace Hyperion
