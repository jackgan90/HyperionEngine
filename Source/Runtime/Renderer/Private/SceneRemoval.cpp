#include "RenderSceneInternal.h"
#include <algorithm>

namespace Hyperion
{
void FRenderSceneClient::AttachLogicalScene(std::uint64_t InIdentity)
{
	RequireMain();
	std::lock_guard Lock(Mailbox->Admission);
	if (Mailbox->bClosed || Mailbox->LogicalScene || !InIdentity)
	{
		throw std::logic_error("Render client already attached or closed");
	}
	Mailbox->LogicalScene = InIdentity;
}

void FRenderSceneClient::DetachLogicalScene(std::uint64_t InIdentity)
{
	RequireMain();
	std::lock_guard Lock(Mailbox->Admission);
	if (Mailbox->LogicalScene == InIdentity)
	{
		Mailbox->LogicalScene = 0;
	}
}

FTaskHandle FRenderSceneClient::RemoveBatch(std::span<FRenderBinding> InBindings)
{
	RequireMain();
	for (const auto& Binding : InBindings)
	{
		if (Binding.Mailbox && Binding.Mailbox != Mailbox)
		{
			throw std::invalid_argument("Foreign binding in removal batch");
		}
	}
	return FRenderBinding::RemoveBatch(InBindings);
}

FTaskHandle FRenderBinding::RemoveBatch(std::span<FRenderBinding> InBindings)
{
	const auto First = std::find_if(InBindings.begin(), InBindings.end(),
	                                [](const FRenderBinding& InBinding)
	                                {
		                                return bool(InBinding.Mailbox);
	                                });
	if (First == InBindings.end())
	{
		return InBindings.empty() ? FTaskHandle{} : InBindings.front().Removal;
	}
	const auto SharedMailbox = First->Mailbox;
	auto& Queue = *SharedMailbox;
	std::lock_guard Lock(Queue.Admission);
	if (!Queue.bClosed)
	{
		Queue.Tasks.Require({EDomain::Main});
	}
	std::vector<std::pair<FRenderPrimitiveHandle, std::shared_ptr<FRenderBindingResult>>> Removals;
	for (auto& Binding : InBindings)
	{
		if (Binding.Mailbox && Binding.Mailbox != SharedMailbox)
		{
			throw std::invalid_argument("Foreign binding in removal batch");
		}
		if (Binding.Mailbox)
		{
			Removals.emplace_back(Binding.Handle, Binding.Result);
		}
	}
	if (!Queue.bClosed && !Removals.empty())
	{
		Queue.Last = Queue.Tasks.Dispatch({EDomain::Render},
		                                  [State = SharedMailbox, Removals]
		                                  {
			                                  for (const auto& [Handle, Result] : Removals)
			                                  {
				                                  State->Scene->Remove(Handle);
				                                  Result->Publish(ERenderPrimitiveStatus::Removed, 0);
			                                  }
		                                  });
		for (const auto& [Handle, Result] : Removals)
		{
			Queue.Active[Handle.Slot] = false;
		}
	}
	for (auto& Binding : InBindings)
	{
		Binding.Removal = Queue.Last;
		Binding.Mailbox.reset();
	}
	return Queue.Last;
}
} // namespace Hyperion
