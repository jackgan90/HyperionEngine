#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
FSceneHandle FSceneEditDocument::CommitDuplicate(FSceneHandle InHandle)
{
	auto& Scene = Target();
	if (!Scene.FindNode(InHandle))
	{
		throw FSceneEditError("stale_handle", "Duplicate target no longer exists");
	}
	FSceneHistoryEntry Entry;
	Entry.BeforeSettings = Entry.AfterSettings = Scene.Settings();
	Entry.BeforeState = State.State;
	Entry.AfterState = State.NextState + 1;
	Entry.BeforeSelection = Selected;
	Entry.bRestoreSelection = true;
	State.History.reserve(State.HistoryCursor + 1);
	const auto Handle = Scene.DuplicateNode(InHandle);
	try
	{
		Entry.After = *Scene.FindNode(Handle);
	}
	catch (...)
	{
		const std::array Roots{Handle};
		Scene.RemoveSubtrees(Roots);
		throw;
	}
	Entry.Handle = Handle;
	FinishInteraction();
	++State.NextState;
	Append(std::move(Entry));
	ReplaceSelection(FSceneSelection(Handle));
	return Handle;
}

void FSceneEditDocument::CommitRemoveKeepChildren(FSceneHandle InHandle)
{
	auto& Scene = Target();
	const auto* Node = Scene.FindNode(InHandle);
	if (!Node)
	{
		throw FSceneEditError("stale_handle", "Delete target no longer exists");
	}
	FSceneHistoryEntry Entry{InHandle, {}, {}, Scene.Settings(), {}, State.State, State.NextState + 1};
	Entry.bKeepChildren = true;
	Entry.BeforeSelection = Selected;
	Entry.DeletedSubtree.emplace_back(InHandle, *Node);
	for (const auto Child : Scene.Children(InHandle))
	{
		Entry.Edits.push_back({Child, *Scene.FindNode(Child), *Scene.FindNode(Child)});
	}
	State.History.reserve(State.HistoryCursor + 1);
	if (!Scene.RemoveNodeKeepChildren(InHandle))
	{
		throw FSceneEditError("stale_handle", "Delete target no longer exists");
	}
	Entry.AfterSettings = Scene.Settings();
	FinishInteraction();
	++State.NextState;
	Append(std::move(Entry));
	if (Selected.Contains(InHandle))
	{
		Selected.Toggle(InHandle);
	}
	NotifyHistory();
}

void FSceneEditDocument::CommitReparent(FSceneHandle InHandle, std::optional<FSceneHandle> InParent,
                                        ESceneReparentMode InMode)
{
	auto& Scene = Target();
	FSceneNodeView View;
	FSceneNodeView Parent;
	if (!Scene.NodeView(InHandle, View) || (InParent && !Scene.NodeView(*InParent, Parent)))
	{
		throw FSceneEditError("stale_handle", "Reparent requires current node and parent handles");
	}
	if (InMode != ESceneReparentMode::KeepLocal && InMode != ESceneReparentMode::KeepWorld)
	{
		throw std::invalid_argument("Invalid scene reparent mode");
	}
	auto Node = *View.Node;
	const auto ParentId = InParent ? Parent.Node->Id : std::string{};
	if (Node.Parent() == ParentId)
	{
		return;
	}
	Node.Parent() = ParentId;
	if (InMode == ESceneReparentMode::KeepWorld)
	{
		const auto ParentInverse = InParent ? Inverse(Parent.World) : Hyperion::Identity();
		if (!IsAffine(ParentInverse))
		{
			throw std::invalid_argument("Reparent requires an invertible parent");
		}
		Node.Local() = Multiply(ParentInverse, View.World);
	}
	CommitEdits({{InHandle, std::move(Node)}}, Scene.Revision());
}
} // namespace Hyperion
