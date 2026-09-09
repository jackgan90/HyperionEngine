#include "RenderSceneInternal.h"
#include <algorithm>
#include <atomic>
#include <exception>
#include <set>
#include <stdexcept>

namespace Hyperion
{
void FRenderBindingResult::Publish(ERenderPrimitiveStatus InState, std::uint64_t InRevision, std::string InError,
                                   std::shared_ptr<const FRenderResource> InResource, std::uint32_t InSection,
                                   std::shared_ptr<const FRenderPrimitiveState> InAdmitted)
{
	std::shared_ptr<const FRenderPrimitiveState> Previous;
	{
		std::lock_guard Lock(Mutex);
		Status = {InState, InRevision, std::move(InError)};
		Resource = std::move(InResource);
		Section = InSection;
		Previous = std::move(Admitted);
		Admitted = std::move(InAdmitted);
		LastDraw = {};
		LastDrawFrame.reset();
		ValidatedSchema.reset();
	}
	// Lease destruction can notify the resource coordinator; never do it under the result mutex.
}

FRenderScene::~FRenderScene()
{
	if (!Tasks.IsCurrent({EDomain::Render}))
	{
		std::terminate();
	}
	for (auto& [Slot, Entry] : Entries)
	{
		Entry.Primitive.reset();
		Entry.Result->Publish(ERenderPrimitiveStatus::Removed, 0);
	}
}

void FRenderScene::Invalidate()
{
	++Revision;
	Cacheable.reset();
	if (OnChanged)
	{
		OnChanged();
	}
}

std::optional<std::uint64_t> FRenderScene::GetCollectionRevision() const
{
	Tasks.Require({EDomain::Render});
	if (!Cacheable)
	{
		Cacheable = std::all_of(Entries.begin(), Entries.end(),
		                        [](const auto& InEntry)
		                        {
			                        return InEntry.second.Primitive->IsStaticCollection();
		                        });
	}
	return *Cacheable ? std::optional(Revision) : std::nullopt;
}

void FRenderScene::Create(FRenderPrimitiveHandle InHandle, FRenderPrimitiveState InState, std::uint64_t InGroup,
                          const FRenderPrimitiveFactory& InFactory, std::shared_ptr<FRenderBindingResult> InResult)
{
	Tasks.Require({EDomain::Render});
	Invalidate();
	try
	{
		ValidatePrimitiveState(InState);
		auto Primitive = InFactory ? InFactory(Tasks) : std::make_unique<FStaticMeshRenderPrimitive>(Tasks);
		if (!Primitive)
		{
			throw std::invalid_argument("Primitive factory returned no object");
		}
		const auto Admitted = std::make_shared<const FRenderPrimitiveState>(InState);
		const auto Revision = InState.Revision;
		const bool bPending = bool(InState.Resource);
		Primitive->Apply(std::move(InState));
		Entries.emplace(InHandle.Slot, FEntry{InHandle, std::move(Primitive), InResult, InGroup, {}, ScopeFactory()});
		Groups[InGroup].insert(InHandle.Slot);
		DirtyGroups.insert(InGroup);
		const auto& State = Entries.at(InHandle.Slot).Primitive->GetState();
		InResult->Publish(bPending ? ERenderPrimitiveStatus::PendingResources : ERenderPrimitiveStatus::Ready, Revision,
		                  {}, State.Resource, State.Section, Admitted);
	}
	catch (const std::exception& Error)
	{
		InResult->Publish(ERenderPrimitiveStatus::Failed, 0, Error.what());
	}
	catch (...)
	{
		InResult->Publish(ERenderPrimitiveStatus::Failed, 0, "Primitive construction failed");
	}
}

void FRenderScene::Update(std::vector<FRenderPrimitiveUpdate> InUpdates)
{
	Tasks.Require({EDomain::Render});
	std::set<std::uint32_t> Seen;
	std::map<std::uint32_t, std::shared_ptr<const void>> Lifetimes;
	std::map<std::uint32_t, std::shared_ptr<const FRenderPrimitiveState>> Admitted;
	std::map<std::uint32_t, std::shared_ptr<FMaterialEvaluationCache>> Caches;
	// Validate the entire transaction before touching any live state. Move-only publication cannot throw.
	for (const auto& Update : InUpdates)
	{
		const auto It = Entries.find(Update.Handle.Slot);
		if (It == Entries.end() || It->second.Handle != Update.Handle ||
		    Update.State.Revision <= It->second.Primitive->GetState().Revision)
		{
			return;
		}
		ValidatePrimitiveState(Update.State);
		const auto& Previous = It->second.Primitive->GetState();
		const bool bChanged = Previous.World.Values != Update.State.World.Values ||
		                      Previous.ObjectParameters != Update.State.ObjectParameters ||
		                      Previous.SectionParameters != Update.State.SectionParameters ||
		                      Previous.ObjectInputs != Update.State.ObjectInputs;
		Lifetimes.emplace(Update.Handle.Slot, bChanged ? ScopeFactory() : It->second.Lifetime);
		Admitted.emplace(Update.Handle.Slot, std::make_shared<const FRenderPrimitiveState>(Update.State));
		Caches.emplace(Update.Handle.Slot, std::make_shared<FMaterialEvaluationCache>());
		DirtyGroups.insert(It->second.Group);
		if (!Seen.insert(Update.Handle.Slot).second)
		{
			throw std::invalid_argument("Duplicate primitive in update batch");
		}
	}
	if (!InUpdates.empty())
	{
		Invalidate();
	}
	for (auto& Update : InUpdates)
	{
		auto& Entry = Entries.at(Update.Handle.Slot);
		const auto Revision = Update.State.Revision;
		const bool bPending = bool(Update.State.Resource);
		Entry.Lifetime = std::move(Lifetimes.at(Update.Handle.Slot));
		Entry.EvaluationCache = std::move(Caches.at(Update.Handle.Slot));
		Entry.Primitive->Apply(std::move(Update.State));
		const auto& State = Entry.Primitive->GetState();
		Entry.Result->Publish(bPending ? ERenderPrimitiveStatus::PendingResources : ERenderPrimitiveStatus::Ready,
		                      Revision, {}, State.Resource, State.Section, Admitted.at(Update.Handle.Slot));
	}
}

void FRenderScene::Remove(FRenderPrimitiveHandle InHandle)
{
	Tasks.Require({EDomain::Render});
	const auto It = Entries.find(InHandle.Slot);
	if (It != Entries.end() && It->second.Handle == InHandle)
	{
		Invalidate();
		auto Result = It->second.Result;
		const auto Group = It->second.Group;
		Groups.at(Group).erase(InHandle.Slot);
		if (Groups.at(Group).empty())
		{
			Groups.erase(Group);
			Spatial->Remove(Group);
			DirtyGroups.erase(Group);
			UnboundedGroups.erase(Group);
		}
		else
		{
			DirtyGroups.insert(Group);
		}
		Entries.erase(It);
		Result->Publish(ERenderPrimitiveStatus::Removed, 0);
	}
}

FRenderSceneMailbox::FRenderSceneMailbox(FTaskSystem& InTasks) : Tasks(InTasks)
{
	static std::atomic_uint64_t NextIdentity{1};
	Identity = NextIdentity.fetch_add(1);
}

FTaskHandle FRenderSceneMailbox::Remove(FRenderPrimitiveHandle InHandle,
                                        const std::shared_ptr<FRenderBindingResult>& InResult)
{
	std::lock_guard Lock(Admission);
	if (bClosed)
	{
		return Last;
	}
	Tasks.Require({EDomain::Main});
	if (InHandle.Scene != Identity || InHandle.Slot >= Active.size() ||
	    Generations[InHandle.Slot] != InHandle.Generation || !Active[InHandle.Slot])
	{
		return {};
	}
	Last = Tasks.Dispatch({EDomain::Render},
	                      [this, InHandle, InResult]
	                      {
		                      Scene->Remove(InHandle);
		                      InResult->Publish(ERenderPrimitiveStatus::Removed, 0);
	                      });
	Active[InHandle.Slot] = false;
	return Last;
}
} // namespace Hyperion
