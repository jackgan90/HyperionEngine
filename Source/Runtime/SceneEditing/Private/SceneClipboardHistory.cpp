#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
void FSceneEditDocument::CommitClipboardBatch(std::vector<FSceneNode> InNodes)
{
	FSceneHistoryEntry Entry;
	Entry.BeforeState = State.State;
	Entry.AfterState = State.NextState + 1;
	Entry.BeforeSettings = Entry.AfterSettings = Target().Settings();
	Entry.BeforeSelection = Selected;
	Entry.AfterSelection = ClipboardSnapshot->Selection;
	auto AfterSelection = Entry.AfterSelection;
	Entry.CreatedRoots = ClipboardSnapshot->Roots;
	Entry.CreatedNodes.reserve(InNodes.size());
	FSceneHandleMap Mapping;
	Mapping.reserve(InNodes.size());
	for (std::size_t Index = 0; Index < InNodes.size(); ++Index)
	{
		const auto Original = ClipboardSnapshot->Nodes[Index].first;
		Entry.CreatedNodes.emplace_back(Original, InNodes[Index]);
		Mapping.emplace(Original, FSceneHandle{});
	}
	State.History.reserve(State.HistoryCursor + 1);
	const auto Handles = Target().AddNodes(std::move(InNodes));
	for (std::size_t Index = 0; Index < Handles.size(); ++Index)
	{
		auto& Original = Entry.CreatedNodes[Index].first;
		Mapping.at(Original) = Handles[Index];
		Original = Handles[Index];
	}
	for (auto& Root : Entry.CreatedRoots)
	{
		Root = Mapping.at(Root);
	}
	Entry.AfterSelection.RemapFreshHandles(Mapping);
	AfterSelection.RemapFreshHandles(Mapping);
	Selected = std::move(AfterSelection);
	FinishInteraction();
	++State.NextState;
	Append(std::move(Entry));
	NotifyHistory();
}

void FSceneEditDocument::RestoreCreatedBatch(std::size_t InIndex, bool bInAfter)
{
	auto& Entry = State.History.at(InIndex);
	if (!bInAfter)
	{
		auto BeforeSelection = Entry.BeforeSelection;
		if (!Target().RemoveSubtrees(Entry.CreatedRoots))
		{
			throw FSceneEditError("stale_handle", "Pasted objects no longer exist");
		}
		Selected = std::move(BeforeSelection);
		return;
	}
	std::vector<FSceneNode> Nodes;
	auto AfterSelection = Entry.AfterSelection;
	FSceneHandleMap Mapping;
	Mapping.reserve(Entry.CreatedNodes.size());
	for (const auto& [Handle, Node] : Entry.CreatedNodes)
	{
		Nodes.push_back(bAssetRefreshHistory ? Target().Rebind(Node) : Node);
		Mapping.emplace(Handle, FSceneHandle{});
	}
	const auto Handles = Target().AddNodes(std::move(Nodes));
	for (std::size_t Index = 0; Index < Handles.size(); ++Index)
	{
		Mapping.at(Entry.CreatedNodes[Index].first) = Handles[Index];
	}
	RemapHistory(Mapping);
	AfterSelection.RemapFreshHandles(Mapping);
	Selected = std::move(AfterSelection);
}
} // namespace Hyperion
