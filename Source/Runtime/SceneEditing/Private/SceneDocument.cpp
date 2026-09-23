#include "Hyperion/SceneEditing/SceneDocument.h"
#include "Hyperion/Core/Identity.h"
#include <utility>

namespace Hyperion
{
FSceneEditDocument::FSceneEditDocument() : Identity(CreateEphemeralIdentity())
{
}

FSceneEditError::FSceneEditError(std::string InCode, std::string InMessage)
    : std::runtime_error(std::move(InMessage)), Code(std::move(InCode))
{
}

void FSceneEditDocument::Attach(ISceneEditTarget& InTarget, bool bInHistory)
{
	if (EditTarget)
	{
		throw std::logic_error("Scene document is already attached");
	}
	EditTarget = &InTarget;
	bHistory = bInHistory;
	Reset();
}

void FSceneEditDocument::Detach(FTaskSystem& InTasks)
{
	Observer = {};
	if (State.Save)
	{
		try
		{
			InTasks.Wait(State.Save->Result.Task());
		}
		catch (const std::exception&)
		{
			// PollSave retains the terminal error for the owning consumer.
		}
		PollSave();
	}
	Reset();
	EditTarget = nullptr;
}

ISceneEditTarget& FSceneEditDocument::Target() const
{
	if (!EditTarget)
	{
		throw FSceneEditError("unavailable", "No scene editing target is attached");
	}
	return *EditTarget;
}

const FSceneDocumentState& FSceneEditDocument::GetState() const
{
	return State;
}

FSceneSelection& FSceneEditDocument::Selection()
{
	return Selected;
}

const FSceneSelection& FSceneEditDocument::Selection() const
{
	return Selected;
}

std::string FSceneEditDocument::Id() const
{
	return Identity + "/" + std::to_string(Target().Identity()) + "/" + std::to_string(State.Epoch);
}

bool FSceneEditDocument::HasHistory() const
{
	return bHistory;
}

bool FSceneEditDocument::IsDirty() const
{
	return bPreviewDirty || State.State != State.SavedState;
}

bool FSceneEditDocument::IsBusy() const
{
	return bBusy || State.Interaction.has_value();
}

void FSceneEditDocument::SetInteractionState(bool bInBusy, bool bInPreviewDirty)
{
	bBusy = bInBusy;
	bPreviewDirty = bInPreviewDirty;
}

void FSceneEditDocument::RequireIdle(const std::string& InDocument, std::uint64_t InRevision) const
{
	if (InDocument != Id())
	{
		throw FSceneEditError("stale_document", "Scene document was replaced; query the current scene");
	}
	if (!Target().IsLoaded())
	{
		throw FSceneEditError("unavailable", "The scene has not loaded");
	}
	if (IsBusy() || !Target().IsReady())
	{
		throw FSceneEditError("busy", "Scene preparation, an interaction or a modal operation is active");
	}
	if (InRevision != Target().Revision())
	{
		throw FSceneEditError("stale_revision", "Scene changed; query current values before editing");
	}
}

void FSceneEditDocument::Reset()
{
	State.History.clear();
	State.HistoryCursor = 0;
	State.State = State.SavedState = ++State.NextState;
	++State.Epoch;
	bAssetRefreshHistory = false;
	FinishInteraction();
	bBusy = bPreviewDirty = false;
	Remapped.clear();
}

void FSceneEditDocument::Invalidate()
{
	++State.Epoch;
}

void FSceneEditDocument::SetPath(std::string InPath)
{
	State.Path = std::move(InPath);
}

void FSceneEditDocument::MarkSaved(std::uint64_t InEpoch, std::uint64_t InState)
{
	if (InEpoch == State.Epoch)
	{
		State.SavedState = InState;
	}
}

void FSceneEditDocument::AssetsRefreshed()
{
	bAssetRefreshHistory = true;
}

void FSceneEditDocument::FinishInteraction()
{
	State.Interaction.reset();
}

void FSceneEditDocument::Append(FSceneHistoryEntry InEntry)
{
	State.State = InEntry.AfterState;
	if (bHistory)
	{
		State.History.resize(State.HistoryCursor);
		State.History.push_back(std::move(InEntry));
		++State.HistoryCursor;
	}
}

void FSceneEditDocument::CommitEdits(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision,
                                     std::uint64_t InInteraction)
{
	auto& Scene = Target();
	if (Scene.Revision() != InExpectedRevision)
	{
		throw FSceneEditError("stale_revision", "The objects changed while editing. Query their current values.");
	}
	FSceneHistoryEntry Entry;
	Entry.BeforeSettings = Scene.Settings();
	Entry.BeforeState = State.State;
	std::vector<FSceneHandle> Targets;
	bool bChanged{};
	for (const auto& [Handle, Candidate] : InEdits)
	{
		const auto* Before = Scene.FindNode(Handle);
		if (!Before)
		{
			throw FSceneEditError("stale_handle", "An edit target is no longer current");
		}
		bChanged |= *Before != Candidate;
		Targets.push_back(Handle);
		Entry.Edits.push_back({Handle, *Before, Candidate});
	}
	if (InEdits.empty() || (!bChanged && !InInteraction))
	{
		return;
	}
	const auto& Interaction = State.Interaction;
	const bool bMerge = bHistory && InInteraction && Interaction && Interaction->Interaction == InInteraction &&
	                    Interaction->Targets == Targets && Interaction->Revision == InExpectedRevision &&
	                    State.HistoryCursor == State.History.size() &&
	                    Interaction->HistoryIndex + 1 == State.HistoryCursor;
	State.History.reserve(State.HistoryCursor + 1);
	if (!Scene.EditNodes(std::move(InEdits), InExpectedRevision))
	{
		throw FSceneEditError("stale_handle", "The edit targets are no longer current");
	}
	Entry.AfterSettings = Scene.Settings();
	Entry.AfterState = ++State.NextState;
	State.State = Entry.AfterState;
	if (bMerge)
	{
		auto& Previous = State.History.back();
		for (std::size_t Index = 0; Index < Entry.Edits.size(); ++Index)
		{
			Previous.Edits[Index].After = std::move(Entry.Edits[Index].After);
		}
		Previous.AfterSettings = std::move(Entry.AfterSettings);
		Previous.AfterState = Entry.AfterState;
	}
	else
	{
		Append(std::move(Entry));
	}
	if (InInteraction && bHistory)
	{
		State.Interaction = FSceneInspectorTransaction{InInteraction, Targets.front(), State.HistoryCursor - 1,
		                                               Scene.Revision(), std::move(Targets)};
	}
}

FSceneHandle FSceneEditDocument::CommitCreate(FSceneNode InNode, bool bInAssignMainLight)
{
	FinishInteraction();
	auto& Scene = Target();
	do
	{
		InNode.Id = "object-" + std::to_string(++State.NextState);
	} while (Scene.FindHandle(InNode.Id).Scene);
	FSceneHistoryEntry Entry{{}, {}, InNode, Scene.Settings(), Scene.Settings(), State.State, State.NextState};
	State.History.reserve(State.HistoryCursor + 1);
	Entry.Handle = Scene.AddNode(std::move(InNode));
	try
	{
		if (bInAssignMainLight && Entry.After->DirectionalLight() && !Entry.BeforeSettings.MainDirectionalLight)
		{
			Entry.AfterSettings.MainDirectionalLight = Entry.Handle;
			Scene.SetSettings(Entry.AfterSettings);
		}
	}
	catch (...)
	{
		const std::array Roots{Entry.Handle};
		Scene.RemoveSubtrees(Roots);
		throw;
	}
	Selected = Entry.Handle;
	const auto Handle = Entry.Handle;
	Append(std::move(Entry));
	return Handle;
}

void FSceneEditDocument::CommitSettings(FSceneSettings InSettings)
{
	FinishInteraction();
	auto& Scene = Target();
	if (Scene.Settings() == InSettings)
	{
		return;
	}
	FSceneHistoryEntry Entry{{}, {}, {}, Scene.Settings(), InSettings, State.State, ++State.NextState};
	State.History.reserve(State.HistoryCursor + 1);
	Scene.SetSettings(std::move(InSettings));
	Append(std::move(Entry));
}

std::vector<FSceneHandle> FSceneEditDocument::SelectedRoots() const
{
	std::vector<FSceneHandle> Roots;
	for (const auto Handle : Selected.All())
	{
		const auto* Node = Target().FindNode(Handle);
		bool bCovered = !Node;
		while (Node && !Node->Parent().empty())
		{
			const auto Parent = Target().FindHandle(Node->Parent());
			bCovered |= Selected.Contains(Parent);
			Node = Target().FindNode(Parent);
		}
		if (!bCovered)
		{
			Roots.push_back(Handle);
		}
	}
	return Roots;
}

void FSceneEditDocument::CommitDelete()
{
	FinishInteraction();
	auto& Scene = Target();
	if (!Selected || !Scene.FindNode(*Selected))
	{
		return;
	}
	FSceneHistoryEntry Entry{*Selected, {}, {}, Scene.Settings(), {}, State.State, ++State.NextState};
	Entry.BeforeSelection = Selected;
	Entry.DeletedRoots = SelectedRoots();
	std::vector<FSceneHandle> Pending = Entry.DeletedRoots;
	for (std::size_t Index = 0; Index < Pending.size(); ++Index)
	{
		const auto Handle = Pending[Index];
		Entry.DeletedSubtree.emplace_back(Handle, *Scene.FindNode(Handle));
		const auto Children = Scene.Children(Handle);
		Pending.insert(Pending.end(), Children.begin(), Children.end());
	}
	State.History.reserve(State.HistoryCursor + 1);
	if (!Scene.RemoveSubtrees(Entry.DeletedRoots))
	{
		throw FSceneEditError("stale_handle", "Delete target no longer exists");
	}
	Entry.AfterSettings = Scene.Settings();
	Selected.Clear();
	Append(std::move(Entry));
	NotifyHistory();
}
} // namespace Hyperion
