#include "RenderSceneInternal.h"
#include <algorithm>
#include <stdexcept>

namespace Hyperion
{
namespace
{
struct FCreation
{
	FRenderPrimitiveHandle Handle;
	FRenderPrimitiveState State;
	std::shared_ptr<FRenderBindingResult> Result;
	std::uint64_t Group{};
	std::size_t OutputGroup{};
};

std::size_t ValidatePublication(const std::vector<std::vector<FRenderPrimitiveState>>& InGroups,
                                const std::vector<FRenderPrimitiveUpdate>& InUpdates,
                                const std::vector<FRenderPrimitiveHandle>& InRemovals)
{
	std::size_t Count{};
	for (std::size_t Index = 0; Index < InGroups.size(); ++Index)
	{
		Count += InGroups[Index].size();
		for (const auto& State : InGroups[Index])
		{
			ValidatePrimitiveState(State);
		}
	}
	for (const auto& Update : InUpdates)
	{
		ValidatePrimitiveState(Update.State);
	}
	std::set<std::tuple<std::uint64_t, std::uint32_t, std::uint64_t>> Handles;
	for (const auto& Update : InUpdates)
	{
		if (!Handles.emplace(Update.Handle.Scene, Update.Handle.Slot, Update.Handle.Generation).second)
		{
			throw std::invalid_argument("Duplicate material publication update");
		}
	}
	for (const auto Handle : InRemovals)
	{
		if (!Handles.emplace(Handle.Scene, Handle.Slot, Handle.Generation).second)
		{
			throw std::invalid_argument("Conflicting material publication removal");
		}
	}
	return Count;
}
} // namespace

FRenderScenePublication FRenderSceneClient::PublishGroups(std::vector<std::vector<FRenderPrimitiveState>> InGroups,
                                                          std::vector<FRenderPrimitiveUpdate> InUpdates,
                                                          std::vector<FRenderPrimitiveHandle> InRemovals)
{
	auto& Queue = *Mailbox;
	Queue.Tasks.Require({EDomain::Main});

	std::vector<FCreation> Creations;
	FRenderScenePublication Result;
	Result.Groups.resize(InGroups.size());
	const auto Count = ValidatePublication(InGroups, InUpdates, InRemovals);
	for (std::size_t Index = 0; Index < InGroups.size(); ++Index)
	{
		Result.Groups[Index].reserve(InGroups[Index].size());
	}
	Creations.reserve(Count);
	{
		std::lock_guard Lock(Queue.Admission);
		if (Queue.bClosed)
		{
			throw std::logic_error("Render scene is closed");
		}
		for (const auto& Update : InUpdates)
		{
			if (Update.Handle.Scene != Queue.Identity || Update.Handle.Slot >= Queue.Active.size() ||
			    !Queue.Active[Update.Handle.Slot] || Queue.Generations[Update.Handle.Slot] != Update.Handle.Generation)
			{
				throw std::invalid_argument("Material publication contains a stale primitive handle");
			}
		}
		Queue.Generations.reserve(Queue.Generations.size() + Count);
		Queue.Active.reserve(Queue.Active.size() + Count);
		try
		{
			for (std::size_t Index = 0; Index < InGroups.size(); ++Index)
			{
				const auto Group = ++Queue.NextGroup;
				for (auto& State : InGroups[Index])
				{
					const auto Slot = static_cast<std::uint32_t>(
					    std::find(Queue.Active.begin(), Queue.Active.end(), false) - Queue.Active.begin());
					if (Slot == Queue.Active.size())
					{
						Queue.Generations.push_back(0);
						Queue.Active.push_back(false);
					}
					const FRenderPrimitiveHandle Handle{Queue.Identity, Slot, ++Queue.Generations[Slot]};
					Creations.push_back(
					    {Handle, std::move(State), std::make_shared<FRenderBindingResult>(), Group, Index});
					Queue.Active[Slot] = true;
				}
			}
			Result.Task = Queue.Tasks.Dispatch(
			    {EDomain::Render},
			    [Mailbox = Mailbox, Creations, Updates = std::move(InUpdates), Removals = InRemovals]() mutable
			    {
				    Mailbox->Scene->Update(std::move(Updates));
				    for (const auto Handle : Removals)
				    {
					    Mailbox->Scene->Remove(Handle);
				    }
				    for (auto& Creation : Creations)
				    {
					    Mailbox->Scene->Create(Creation.Handle, std::move(Creation.State), Creation.Group, {},
					                           Creation.Result);
				    }
			    });
			Queue.Last = Result.Task;
			for (const auto Handle : InRemovals)
			{
				if (Handle.Scene == Queue.Identity && Handle.Slot < Queue.Active.size() &&
				    Queue.Generations[Handle.Slot] == Handle.Generation)
				{
					Queue.Active[Handle.Slot] = false;
				}
			}
		}
		catch (...)
		{
			for (const auto& Creation : Creations)
			{
				Queue.Active[Creation.Handle.Slot] = false;
			}
			throw;
		}
	}
	for (const auto& Creation : Creations)
	{
		Result.Groups[Creation.OutputGroup].push_back(FRenderBinding(Mailbox, Creation.Handle, Creation.Result));
	}
	return Result;
}
} // namespace Hyperion
