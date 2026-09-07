#include "RenderResourcesInternal.h"
#include <stdexcept>

namespace Hyperion
{
namespace
{
struct FPreparedSceneDraws
{
	std::vector<std::optional<FDrawPacket>> Packets;
	std::vector<bool> Srgb;
	std::map<std::pair<std::uint64_t, std::uint64_t>, std::string> Failures;
	bool bDepth{};
	bool bStencil{};
	ERHIDepthFormat Depth = ERHIDepthFormat::None;
};

FPreparedSceneDraws PrepareDraws(FRenderResourceCoordinator& InOwner, const FRenderSceneSnapshot& InSnapshot)
{
	FPreparedSceneDraws Result;
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
		if (Failed != InPrepared.Failures.end())
		{
			continue;
		}
		if (Passes.empty() || Passes.back().Commands.bSrgbTarget != InPrepared.Srgb[Index])
		{
			Passes.push_back(NewPass(InSnapshot, InPrepared, Passes.size(), InPrepared.Srgb[Index]));
		}
		Passes.back().Commands.Draws.push_back(std::move(*InPrepared.Packets[Index]));
	}
	return Passes;
}
} // namespace

std::vector<FColorPass> FRenderResourceService::BuildPasses(const FRenderSceneSnapshot& InSnapshot)
{
	auto& Owner = *Coordinator;
	Owner.Tasks.Require({EDomain::Rhi, 0});
	std::vector<FColorPass> Passes;
	{
		std::lock_guard Lock(Owner.Mutex);
		if (Owner.bClosed)
		{
			throw std::logic_error("Render resource service is closed");
		}
		Passes = PublishDraws(InSnapshot, PrepareDraws(Owner, InSnapshot));
		if (Owner.MaterialGpu)
		{
			Owner.Stats.Materials = Owner.MaterialGpu->Statistics();
			Owner.Stats.Constants = Owner.MaterialConstants->Statistics();
		}
	}
	Owner.Schedule();
	return Passes;
}
} // namespace Hyperion
