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

FRenderDrawResult FRenderBinding::GetLastDrawResult() const
{
	if (!Result)
	{
		return {};
	}
	std::lock_guard Lock(Result->Mutex);
	auto Draw = Result->LastDraw;
	if (Result->LastDrawFrame)
	{
		Draw.Frame = std::max(Draw.Frame, Result->LastDrawFrame->load(std::memory_order_acquire));
	}
	return Draw;
}

FRenderBindingStatus FRenderBinding::GetStatus() const
{
	if (!Result)
	{
		return {ERenderPrimitiveStatus::Removed, 0, {}};
	}
	FRenderBindingStatus Status;
	std::shared_ptr<const FMaterialParameterSchema> ValidatedSchema;
	std::shared_ptr<const FRenderResource> ResourceLease;
	std::shared_ptr<const FRenderPrimitiveState> Admitted;
	std::uint32_t Section{};
	{
		std::lock_guard Lock(Result->Mutex);
		Status = Result->Status;
		ResourceLease = Result->Resource.lock();
		Admitted = Result->Admitted;
		Section = Result->Section;
		ValidatedSchema = Result->ValidatedSchema;
	}
	if (Status.State == ERenderPrimitiveStatus::PendingResources)
	{
		if (auto Resource = ResourceLease)
		{
			const auto State = Resource->GetStatus();
			if (State == ERenderResourceStatus::Ready)
			{
				const auto Description = Resource->GetDescription();
				if (Description)
				{
					const bool bValid = Section < Description->Sections.size();
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
	if (Status.State == ERenderPrimitiveStatus::Ready)
	{
		auto Surface = Admitted ? Admitted->Surface : nullptr;
		if (!Surface)
		{
			if (const auto Resource = ResourceLease)
			{
				Surface = Resource->GetMaterial(Section);
			}
		}
		if (Surface)
		{
			const auto MaterialStatus = Surface->GetStatus();
			if (MaterialStatus == ERenderMaterialStatus::Preparing ||
			    MaterialStatus == ERenderMaterialStatus::Uploading)
			{
				Status.State = ERenderPrimitiveStatus::PendingResources;
			}
			else if (MaterialStatus != ERenderMaterialStatus::Ready)
			{
				Status.State = ERenderPrimitiveStatus::Failed;
				Status.Error = Surface->GetError();
			}
			else if (ValidatedSchema != Surface->GetCompiled()->Interface.Schema)
			{
				try
				{
					auto Snapshot = *Surface->GetSnapshot();
					Snapshot.Schema = Surface->GetCompiled()->Interface.Schema;
					const std::array<std::size_t, 0> NoRequiredParameters{};
					ResolveMaterialParameters(Snapshot, {},
					                          Admitted ? GetPrimitiveMaterialOverrides(*Admitted, *Snapshot.Schema)
					                                   : FMaterialParameterValues{},
					                          {}, NoRequiredParameters);
					std::lock_guard Lock(Result->Mutex);
					if (Result->Admitted == Admitted)
					{
						Result->ValidatedSchema = Snapshot.Schema;
					}
				}
				catch (const std::exception& Error)
				{
					Status.State = ERenderPrimitiveStatus::Failed;
					Status.Error = Error.what();
				}
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

FRenderSceneClient::FRenderSceneClient(FTaskSystem& InTasks,
                                       std::function<std::shared_ptr<const void>()> InScopeFactory,
                                       std::function<void()> InOnChanged)
    : Mailbox(std::make_shared<FRenderSceneMailbox>(InTasks))
{
	InTasks.Require({EDomain::Main});
	InTasks.Wait(InTasks.Dispatch(
	    {EDomain::Render},
	    [State = Mailbox, Factory = std::move(InScopeFactory), Changed = std::move(InOnChanged)]() mutable
	    {
		    if (!Factory)
		    {
			    Factory = []
			    {
				    return std::make_shared<const int>(0);
			    };
		    }
		    State->Scene = std::make_unique<FRenderScene>(State->Tasks, std::move(Factory), std::move(Changed));
	    }));
}

FRenderSceneClient::~FRenderSceneClient()
{
	Close();
}

std::optional<std::uint64_t> FRenderSceneClient::GetCollectionRevision() const
{
	Mailbox->Tasks.Require({EDomain::Render});
	if (!Mailbox->Scene)
	{
		throw std::logic_error("Render scene is closed");
	}
	return Mailbox->Scene->GetCollectionRevision();
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
		Queue.Last = Queue.Tasks.Dispatch(
		    {EDomain::Render},
		    [Mailbox = Mailbox, Creations, Group = ++Queue.NextGroup, Factory = std::move(InFactory)]() mutable
		    {
			    for (auto& Creation : Creations)
			    {
				    Mailbox->Scene->Create(Creation.Handle, std::move(Creation.State), Group, Factory, Creation.Result);
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

std::uint64_t FRenderSceneClient::GetLogicalSceneIdentity() const
{
	std::lock_guard Lock(Mailbox->Admission);
	return Mailbox->LogicalScene;
}

FRenderSceneSnapshot FRenderSceneClient::Collect(FRenderView InView, bool bInRefresh, std::uint64_t InResourceRevision,
                                                 FRenderSceneSnapshot* InPrevious) const
{
	Mailbox->Tasks.Require({EDomain::Render});
	if (!Mailbox->Scene)
	{
		throw std::logic_error("Render scene is closed");
	}
	return Mailbox->Scene->Collect(std::move(InView), bInRefresh, InResourceRevision, InPrevious);
}

FSceneVisibilityStats FRenderSceneClient::BeginViews() const
{
	Mailbox->Tasks.Require({EDomain::Render});
	return Mailbox->Scene->BeginViews();
}

std::vector<FBounds> FRenderSceneClient::QueryBounds(const ISceneVisibility& InVisibility) const
{
	Mailbox->Tasks.Require({EDomain::Render});
	return Mailbox->Scene->QueryBounds(InVisibility);
}
} // namespace Hyperion
