#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
void FSceneEditDocument::SetSelectionObserver(std::function<void()> InObserver)
{
	SelectionObserver = std::move(InObserver);
}

void FSceneEditDocument::ReplaceSelection(FSceneSelection InSelection)
{
	Selected = std::move(InSelection);
	if (SelectionObserver)
	{
		SelectionObserver();
	}
}

void FSceneEditDocument::SetHistoryObserver(std::function<void(const FSceneHandleMap&)> InObserver)
{
	Observer = std::move(InObserver);
}

void FSceneEditDocument::NotifyHistory()
{
	if (SelectionObserver)
	{
		SelectionObserver();
	}
	if (Observer)
	{
		Observer(Remapped);
	}
	Remapped.clear();
}

void FSceneEditDocument::RemapHistory(const FSceneHandleMap& InMapping)
{
	RemapSceneHistory(State.History, InMapping);
	Selected.Remap(InMapping);
	Remapped.insert(InMapping.begin(), InMapping.end());
}

void FSceneEditDocument::RestoreDeletedSubtree(std::size_t InIndex)
{
	auto& Entry = State.History.at(InIndex);
	std::vector<FSceneNode> Nodes;
	std::vector<FSceneHandle> Original;
	FSceneHandleMap Mapping;
	Mapping.reserve(Entry.DeletedSubtree.size());
	for (const auto& [Handle, Node] : Entry.DeletedSubtree)
	{
		Original.push_back(Handle);
		Nodes.push_back(bAssetRefreshHistory ? Target().Rebind(Node) : Node);
		Mapping.emplace(Handle, FSceneHandle{});
	}
	const auto Restored = Target().AddNodes(std::move(Nodes));
	for (std::size_t Index = 0; Index < Restored.size(); ++Index)
	{
		Mapping.at(Original[Index]) = Restored[Index];
	}
	RemapHistory(Mapping);
	Selected = Entry.BeforeSelection;
}

void FSceneEditDocument::RestoreHistory(std::size_t InIndex, bool bInAfter)
{
	auto& Scene = Target();
	auto& Entry = State.History.at(InIndex);
	auto Node = bInAfter ? Entry.After : Entry.Before;
	if (bAssetRefreshHistory && Node)
	{
		Node = Scene.Rebind(std::move(*Node));
	}
	if (!Entry.Edits.empty())
	{
		std::vector<FSceneNodeEdit> Edits;
		for (const auto& Edit : Entry.Edits)
		{
			auto Candidate = bInAfter ? Edit.After : Edit.Before;
			if (bAssetRefreshHistory)
			{
				Candidate = Scene.Rebind(std::move(Candidate));
			}
			Edits.push_back({Edit.Handle, std::move(Candidate)});
		}
		if (!Scene.EditNodes(std::move(Edits), Scene.Revision()))
		{
			throw FSceneEditError("stale_handle", "History targets no longer exist");
		}
	}
	else if (!Entry.DeletedSubtree.empty() && !bInAfter)
	{
		RestoreDeletedSubtree(InIndex);
	}
	else if (Entry.Handle.Scene)
	{
		if (!Node)
		{
			const auto Roots = Entry.DeletedRoots.empty() ? std::vector{Entry.Handle} : Entry.DeletedRoots;
			if (!Scene.RemoveSubtrees(Roots))
			{
				throw FSceneEditError("stale_handle", "Undo target no longer exists");
			}
			if (!Entry.DeletedSubtree.empty())
			{
				Selected.Clear();
			}
			else
			{
				auto Updated = Selected;
				for (const auto Handle : Selected.All())
				{
					if (!Scene.FindNode(Handle))
					{
						Updated.Toggle(Handle);
					}
				}
				Selected = std::move(Updated);
			}
		}
		else if (!Entry.Before && bInAfter)
		{
			const auto Handle = Scene.AddNode(*Node);
			RemapHistory(FSceneHandleMap{{Entry.Handle, Handle}});
			Selected = Handle;
		}
		else if (!Scene.EditNodes({{Entry.Handle, *Node}}, Scene.Revision()))
		{
			throw FSceneEditError("stale_handle", "History target no longer exists");
		}
	}
	Scene.SetSettings(bInAfter ? Entry.AfterSettings : Entry.BeforeSettings);
}

void FSceneEditDocument::Undo()
{
	if (!bHistory)
	{
		throw FSceneEditError("unavailable", "This host does not provide scene history");
	}
	FinishInteraction();
	if (!State.HistoryCursor)
	{
		return;
	}
	RestoreHistory(State.HistoryCursor - 1, false);
	--State.HistoryCursor;
	State.State = State.History[State.HistoryCursor].BeforeState;
	if (bAssetRefreshHistory)
	{
		Target().RefreshAssets();
	}
	NotifyHistory();
}

void FSceneEditDocument::Redo()
{
	if (!bHistory)
	{
		throw FSceneEditError("unavailable", "This host does not provide scene history");
	}
	FinishInteraction();
	if (State.HistoryCursor == State.History.size())
	{
		return;
	}
	RestoreHistory(State.HistoryCursor, true);
	State.State = State.History[State.HistoryCursor].AfterState;
	if (bAssetRefreshHistory)
	{
		Target().RefreshAssets();
	}
	++State.HistoryCursor;
	NotifyHistory();
}
} // namespace Hyperion
