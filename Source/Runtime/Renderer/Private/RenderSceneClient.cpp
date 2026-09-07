#include "Hyperion/Renderer/RenderResources.h"
#include "RenderSceneInternal.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace Hyperion
{
FRenderBinding::FRenderBinding(std::shared_ptr<FRenderSceneMailbox> InMailbox, FRenderPrimitiveHandle InHandle,
                               std::shared_ptr<FRenderBindingResult> InResult)
    : Mailbox(std::move(InMailbox)), Handle(InHandle), Result(std::move(InResult))
{
}

FRenderBinding::~FRenderBinding()
{
	Remove();
}

FRenderBinding::FRenderBinding(FRenderBinding&& InOther) noexcept
    : Mailbox(std::move(InOther.Mailbox)), Handle(InOther.Handle), Result(std::move(InOther.Result)),
      Removal(std::move(InOther.Removal))
{
}

FRenderBinding& FRenderBinding::operator=(FRenderBinding&& InOther) noexcept
{
	if (this != &InOther)
	{
		Remove();
		Mailbox = std::move(InOther.Mailbox);
		Handle = InOther.Handle;
		Result = std::move(InOther.Result);
		Removal = std::move(InOther.Removal);
	}
	return *this;
}

FRenderPrimitiveHandle FRenderBinding::GetHandle() const
{
	return Handle;
}

FRenderBindingStatus FRenderBinding::GetStatus() const
{
	if (!Result)
	{
		return {ERenderPrimitiveStatus::Removed, 0, {}};
	}
	std::lock_guard Lock(Result->Mutex);
	auto Status = Result->Status;
	if (Status.State == ERenderPrimitiveStatus::PendingResources)
	{
		if (auto Resource = Result->Resource.lock())
		{
			const auto State = Resource->GetStatus();
			if (State == ERenderResourceStatus::Ready)
			{
				const auto Description = Resource->GetDescription();
				if (Description)
				{
					const bool bValid = Result->Section < Description->Sections.size();
					Status.State = bValid ? ERenderPrimitiveStatus::Ready : ERenderPrimitiveStatus::Failed;
					if (!bValid)
					{
						Status.Error = "Invalid primitive section";
					}
				}
			}
			if (State == ERenderResourceStatus::Failed || State == ERenderResourceStatus::Retired)
			{
				Status.State = ERenderPrimitiveStatus::Failed;
				Status.Error = Resource->GetError();
			}
		}
	}
	return Status;
}

FTaskHandle FRenderBinding::Remove()
{
	if (Mailbox)
	{
		Removal = Mailbox->Remove(Handle, Result);
		Mailbox.reset();
	}
	return Removal;
}

FRenderSceneClient::FRenderSceneClient(FTaskSystem& InTasks) : Mailbox(std::make_shared<FRenderSceneMailbox>(InTasks))
{
	InTasks.Require({EDomain::Main});
	InTasks.Wait(InTasks.Dispatch({EDomain::Render},
	                              [State = Mailbox]
	                              {
		                              State->Scene = std::make_unique<FRenderScene>(State->Tasks);
	                              }));
}

FRenderSceneClient::~FRenderSceneClient()
{
	Close();
}

void FRenderSceneClient::RequireMain() const
{
	Mailbox->Tasks.Require({EDomain::Main});
}

FRenderBinding FRenderSceneClient::Create(FRenderPrimitiveState InState, FRenderPrimitiveFactory InFactory)
{
	auto Bindings = CreateBatch({std::move(InState)}, std::move(InFactory));
	return std::move(Bindings.front());
}

std::vector<FRenderBinding> FRenderSceneClient::CreateBatch(std::vector<FRenderPrimitiveState> InStates,
                                                            FRenderPrimitiveFactory InFactory)
{
	auto& Queue = *Mailbox;
	Queue.Tasks.Require({EDomain::Main});

	struct FCreation
	{
		FRenderPrimitiveHandle Handle;
		FRenderPrimitiveState State;
		std::shared_ptr<FRenderBindingResult> Result;
	};

	std::vector<FRenderBinding> Bindings;
	std::vector<FCreation> Creations;
	Bindings.reserve(InStates.size());
	Creations.reserve(InStates.size());
	std::lock_guard Lock(Queue.Admission);
	if (Queue.bClosed)
	{
		throw std::logic_error("Render scene is closed");
	}
	Queue.Generations.reserve(Queue.Generations.size() + InStates.size());
	Queue.Active.reserve(Queue.Active.size() + InStates.size());
	// Roll back every reservation if allocation or admission fails.
	try
	{
		for (auto& State : InStates)
		{
			const auto It = std::find(Queue.Active.begin(), Queue.Active.end(), false);
			const auto Slot = static_cast<std::uint32_t>(It - Queue.Active.begin());
			if (Slot == Queue.Active.size())
			{
				Queue.Generations.push_back(0);
				Queue.Active.push_back(false);
			}
			const FRenderPrimitiveHandle Handle{Queue.Identity, Slot, ++Queue.Generations[Slot]};
			Creations.push_back({Handle, std::move(State), std::make_shared<FRenderBindingResult>()});
			Queue.Active[Slot] = true;
		}
		// One Render task makes the complete registration visible at a frame boundary.
		Queue.Last = Queue.Tasks.Dispatch({EDomain::Render},
		                                  [Mailbox = Mailbox, Creations, Factory = std::move(InFactory)]() mutable
		                                  {
			                                  for (auto& Creation : Creations)
			                                  {
				                                  Mailbox->Scene->Create(Creation.Handle, std::move(Creation.State),
				                                                         Factory, Creation.Result);
			                                  }
		                                  });
	}
	catch (...)
	{
		for (const auto& Creation : Creations)
		{
			Queue.Active[Creation.Handle.Slot] = false;
		}
		throw;
	}
	for (const auto& Creation : Creations)
	{
		Bindings.push_back(FRenderBinding(Mailbox, Creation.Handle, Creation.Result));
	}
	return Bindings;
}

FTaskHandle FRenderSceneClient::Update(std::vector<FRenderPrimitiveUpdate> InUpdates)
{
	auto& Queue = *Mailbox;
	Queue.Tasks.Require({EDomain::Main});
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
			return {};
		}
	}
	Queue.Last = Queue.Tasks.Dispatch({EDomain::Render},
	                                  [State = Mailbox, Updates = std::move(InUpdates)]() mutable
	                                  {
		                                  State->Scene->Update(std::move(Updates));
	                                  });
	return Queue.Last;
}

FTaskHandle FRenderSceneClient::Flush()
{
	auto& Queue = *Mailbox;
	Queue.Tasks.Require({EDomain::Main});
	std::lock_guard Lock(Queue.Admission);
	if (!Queue.bClosed)
	{
		Queue.Last = Queue.Tasks.Dispatch({EDomain::Render},
		                                  []
		                                  {
		                                  });
	}
	return Queue.Last;
}

void FRenderSceneClient::Close()
{
	auto& Queue = *Mailbox;
	FTaskHandle Completion;
	{
		std::lock_guard Lock(Queue.Admission);
		if (Queue.bClosed)
		{
			return;
		}
		Queue.Tasks.Require({EDomain::Main});
		Queue.Last = Queue.Tasks.Dispatch({EDomain::Render},
		                                  [State = Mailbox]
		                                  {
			                                  State->Scene.reset();
		                                  });
		Queue.bClosed = true;
		Completion = Queue.Last;
	}
	Queue.Tasks.Wait(Completion);
}

FRenderSceneSnapshot FRenderSceneClient::Collect(FRenderView InView) const
{
	Mailbox->Tasks.Require({EDomain::Render});
	if (!Mailbox->Scene)
	{
		throw std::logic_error("Render scene is closed");
	}
	return Mailbox->Scene->Collect(InView);
}
} // namespace Hyperion
