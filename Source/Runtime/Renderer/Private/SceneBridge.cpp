#include "Hyperion/Renderer/SceneBridge.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/Renderer/RenderSession.h"

namespace Hyperion
{
FSceneRenderBridge::FSceneRenderBridge(FScene& InScene, FRenderSession& InSession, FTaskSystem& InTasks)
    : Scene(InScene), Session(InSession), Tasks(InTasks)
{
	Tasks.Require({EDomain::Main});
	Scene.RequireMain();
	Scene.BeginSynchronization();
	try
	{
		Session.GetScene().AttachLogicalScene(Scene.GetIdentity());
	}
	catch (...)
	{
		Scene.EndSynchronization();
		throw;
	}
}

FSceneRenderBridge::~FSceneRenderBridge()
{
	Close();
}

void FSceneRenderBridge::Observe(bool bInWait)
{
	for (auto It = Receipts.begin(); It != Receipts.end();)
	{
		if (!bInWait && !It->Task.Ready())
		{
			++It;
			continue;
		}
		try
		{
			Tasks.Wait(It->Task);
		}
		catch (const std::exception& Error)
		{
			const auto Entry = Attachments.find(It->Handle);
			if (Entry != Attachments.end() && Entry->second.Revision == It->Revision)
			{
				Entry->second.Error = Error.what();
			}
		}
		It = Receipts.erase(It);
		++StatusRevision;
	}
}

void FSceneRenderBridge::Flush()
{
	HYP_PERF_SCOPE_C(Frame, FlushSceneBridge);
	Tasks.Require({EDomain::Main});
	if (bClosed)
	{
		throw std::logic_error("Scene bridge is closed");
	}
	Observe(false);
	const auto Changes = Scene.GetChanges();
	if (Changes.empty() && EditableModels.empty())
	{
		return;
	}
	std::set<FSceneHandle> Affected;
	auto Pending = PrepareChanges(Changes, Affected);
	if (!Affected.empty())
	{
		PublishChanges(Pending, Affected);
		++StatusRevision;
	}
	for (const auto& Change : Changes)
	{
		Scene.Acknowledge(Change.Revision);
	}
}

std::vector<FSceneRenderBridge::FPending> FSceneRenderBridge::PrepareChanges(const std::vector<FSceneChange>& InChanges,
                                                                             std::set<FSceneHandle>& OutAffected)
{
	std::map<FSceneHandle, std::uint64_t> Changed;
	for (const auto& Change : InChanges)
	{
		Changed.emplace(Change.Handle, Change.Revision);
		OutAffected.insert(Change.Handle);
	}
	for (const auto Handle : EditableModels)
	{
		for (const auto& [Material, Revision] : Attachments.at(Handle).EditableMaterials)
		{
			if (Material->GetRevision() != Revision)
			{
				OutAffected.insert(Handle);
				break;
			}
		}
	}

	FModel::FFrozenMaterials Frozen;
	std::vector<FPending> Pending;
	for (const auto Handle : OutAffected)
	{
		const auto* ModelState = Scene.Find(Handle);
		if (!ModelState || !ModelState->Data)
		{
			continue;
		}
		const auto& State = *ModelState;
		std::vector<std::pair<std::uint64_t, std::uint64_t>> Versions;
		std::vector<std::pair<std::shared_ptr<FMaterialInstance>, std::uint64_t>> EditableMaterials;
		const auto Freeze = [&](const FSceneMaterialSelection& InSelection)
		{
			const auto Snapshot = FModel::FreezeSelection(InSelection, Frozen);
			Versions.emplace_back(Snapshot ? Snapshot->Identity : 0, Snapshot ? Snapshot->Revision : 0);
			if (InSelection.Instance)
			{
				EditableMaterials.emplace_back(InSelection.Instance, InSelection.Instance->GetRevision());
			}
		};
		Freeze(State.Surface);
		for (const auto& [Section, Selection] : State.SectionSurfaces)
		{
			Versions.emplace_back(Section, 0);
			Freeze(Selection);
		}
		auto Entry = Attachments.find(Handle);
		if (Entry != Attachments.end() && Entry->second.Data == State.Data && !Changed.contains(Handle) &&
		    Entry->second.MaterialVersions == Versions)
		{
			continue;
		}
		FPending Work;
		Work.Handle = Handle;
		if (Entry == Attachments.end() || Entry->second.Data != State.Data)
		{
			Work.NewModel =
			    std::unique_ptr<FModel>(new FModel(Session.GetScene(), Session.GetResources(), State, true));
			Work.Model = Work.NewModel.get();
		}
		else
		{
			Work.Model = Entry->second.Model.get();
		}
		Work.Update = Work.Model->PrepareState(State, Frozen);
		Work.Versions = std::move(Versions);
		Work.EditableMaterials = std::move(EditableMaterials);
		Pending.push_back(std::move(Work));
	}
	return Pending;
}

void FSceneRenderBridge::PublishChanges(std::vector<FPending>& InPending, const std::set<FSceneHandle>& InAffected)
{
	std::vector<std::vector<FRenderPrimitiveState>> Groups;
	std::vector<FRenderPrimitiveUpdate> Updates;
	for (const auto& Work : InPending)
	{
		if (Work.NewModel)
		{
			auto& Group = Groups.emplace_back();
			for (const auto& Update : Work.Update.Updates)
			{
				Group.push_back(Update.State);
			}
		}
		else
		{
			Updates.insert(Updates.end(), Work.Update.Updates.begin(), Work.Update.Updates.end());
		}
	}
	std::vector<FRenderPrimitiveHandle> Removals;
	for (const auto Handle : InAffected)
	{
		const auto Existing = Attachments.find(Handle);
		if (Existing == Attachments.end())
		{
			continue;
		}
		const auto& Entry = Existing->second;
		const auto State = Scene.Find(Handle);
		if (!State || !State->Data || State->Data != Entry.Data)
		{
			for (const auto& Binding : Entry.Model->Bindings)
			{
				Removals.push_back(Binding.GetHandle());
			}
		}
	}
	// All snapshots, schemas and overrides are prepared before any related Render publication.
	std::map<FSceneHandle, FAttachment> NewAttachments;
	std::set<FSceneHandle> NewEditableModels;
	for (const auto& Work : InPending)
	{
		if (!Attachments.contains(Work.Handle))
		{
			NewAttachments.try_emplace(Work.Handle);
		}
		if (!Work.EditableMaterials.empty() && !EditableModels.contains(Work.Handle))
		{
			NewEditableModels.insert(Work.Handle);
		}
	}
	Receipts.reserve(Receipts.size() + InPending.size());
	auto Publication =
	    InPending.empty() && Removals.empty()
	        ? FRenderScenePublication{}
	        : Session.GetScene().PublishGroups(std::move(Groups), std::move(Updates), std::move(Removals));
	Attachments.merge(NewAttachments); // Preallocated nodes; no allocation after admission.
	EditableModels.merge(NewEditableModels);
	CommitChanges(InPending, Publication, InAffected);
}

void FSceneRenderBridge::CommitChanges(std::vector<FPending>& InPending, FRenderScenePublication& InPublication,
                                       const std::set<FSceneHandle>& InAffected)
{
	std::size_t GroupIndex{};
	for (auto& Work : InPending)
	{
		auto& Entry = Attachments.at(Work.Handle);
		if (Work.NewModel)
		{
			if (Entry.Model)
			{
				Entry.Model->Remove();
			}
			Work.NewModel->Bindings = std::move(InPublication.Groups[GroupIndex++]);
			Entry.Model = std::move(Work.NewModel);
			Entry.Data = Work.Update.State.Data;
		}
		Entry.Model->CommitState(std::move(Work.Update));
		Entry.MaterialVersions = std::move(Work.Versions);
		Entry.EditableMaterials = std::move(Work.EditableMaterials);
		if (Entry.EditableMaterials.empty())
		{
			EditableModels.erase(Work.Handle);
		}
		Entry.Revision = ++NextPublication;
		Entry.Error.clear();
		Receipts.push_back({Work.Handle, InPublication.Task, Entry.Revision});
	}
	for (const auto Handle : InAffected)
	{
		const auto It = Attachments.find(Handle);
		const auto State = Scene.Find(Handle);
		if (It != Attachments.end() && (!State || !State->Data))
		{
			It->second.Model->Remove();
			EditableModels.erase(Handle);
			Attachments.erase(It);
		}
	}
}

void FSceneRenderBridge::Close()
{
	if (bClosed)
	{
		return;
	}
	Tasks.Require({EDomain::Main});
	for (auto& [Handle, Attachment] : Attachments)
	{
		Attachment.Model->Remove();
	}
	Observe(true);
	Attachments.clear();
	EditableModels.clear();
	Tasks.Wait(Session.GetScene().Flush());
	Session.GetScene().DetachLogicalScene(Scene.GetIdentity());
	Scene.EndSynchronization();
	bClosed = true;
}

bool FSceneRenderBridge::IsReady(FSceneHandle InHandle) const
{
	Tasks.Require({EDomain::Main});
	const auto It = Attachments.find(InHandle);
	return It != Attachments.end() && It->second.Error.empty() && It->second.Model->IsReady();
}

std::pair<std::uint64_t, std::uint64_t> FSceneRenderBridge::GetStatusRevision() const
{
	Tasks.Require({EDomain::Main});
	return {StatusRevision, Session.GetResources().GetPublicationRevision()};
}

std::string FSceneRenderBridge::GetError(FSceneHandle InHandle) const
{
	Tasks.Require({EDomain::Main});
	const auto It = Attachments.find(InHandle);
	return It == Attachments.end()     ? std::string{}
	       : !It->second.Error.empty() ? It->second.Error
	                                   : It->second.Model->GetError();
}

std::vector<FRenderDrawResult> FSceneRenderBridge::GetDrawResults(FSceneHandle InHandle) const
{
	Tasks.Require({EDomain::Main});
	const auto It = Attachments.find(InHandle);
	return It == Attachments.end() ? std::vector<FRenderDrawResult>{} : It->second.Model->GetDrawResults();
}

std::size_t FSceneRenderBridge::PrimitiveCount(FSceneHandle InHandle) const
{
	Tasks.Require({EDomain::Main});
	const auto It = Attachments.find(InHandle);
	return It == Attachments.end() ? 0 : It->second.Model->PrimitiveCount();
}
} // namespace Hyperion
