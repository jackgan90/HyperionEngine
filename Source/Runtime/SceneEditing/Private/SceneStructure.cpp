#include "Hyperion/SceneEditing/SceneDocument.h"
#include "SceneSelectionRoots.h"

namespace Hyperion
{
namespace
{
FMat4 ReparentInverse(const FMat4& InWorld)
{
	if (!IsAffine(InWorld))
	{
		throw std::invalid_argument("Reparent requires an affine parent transform");
	}
	auto Result = Inverse(InWorld);
	// The general inverse may round 1 in the bottom row; its affine form is known exactly.
	Result.Values[3] = 0;
	Result.Values[7] = 0;
	Result.Values[11] = 0;
	Result.Values[15] = 1;
	return Result;
}
} // namespace

FSceneHandle FSceneEditDocument::CommitDuplicate(FSceneHandle InHandle)
{
	auto& Scene = Target();
	if (!Scene.FindNode(InHandle))
	{
		throw FSceneEditError(SceneEditErrors::StaleHandle, "Duplicate target no longer exists");
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
		throw FSceneEditError(SceneEditErrors::StaleHandle, "Delete target no longer exists");
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
		throw FSceneEditError(SceneEditErrors::StaleHandle, "Delete target no longer exists");
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
		throw FSceneEditError(SceneEditErrors::StaleHandle, "Reparent requires current node and parent handles");
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
		const auto ParentInverse = InParent ? ReparentInverse(Parent.World) : Hyperion::Identity();
		Node.Local() = Multiply(ParentInverse, View.World);
	}
	CommitEdits({{InHandle, std::move(Node)}}, Scene.Revision());
}

std::vector<FSceneNodeEdit> FSceneEditDocument::PrepareReparent(std::span<const FSceneHandle> InHandles,
                                                                std::optional<FSceneHandle> InParent) const
{
	auto& Scene = Target();
	if (InHandles.empty())
	{
		throw std::invalid_argument("Select at least one node to reparent");
	}
	std::unordered_set<FSceneHandle, FSceneHandleHash> Handles;
	for (const auto Handle : InHandles)
	{
		if (!Scene.FindNode(Handle))
		{
			throw FSceneEditError(SceneEditErrors::StaleHandle, "A reparent source no longer exists in this scene");
		}
		if (!Handles.insert(Handle).second)
		{
			throw std::invalid_argument("Duplicate reparent source");
		}
	}
	FSceneNodeView Parent;
	if (InParent && !Scene.NodeView(*InParent, Parent))
	{
		throw FSceneEditError(SceneEditErrors::StaleHandle, "The target parent no longer exists in this scene");
	}
	for (auto Ancestor = InParent; Ancestor;)
	{
		if (Handles.contains(*Ancestor))
		{
			throw std::invalid_argument("Cannot parent a node to itself or its descendants");
		}
		const auto& Id = Scene.FindNode(*Ancestor)->Parent();
		Ancestor = Id.empty() ? std::nullopt : std::optional{Scene.FindHandle(Id)};
	}
	const auto ParentId = InParent ? Parent.Node->Id : std::string{};
	std::vector<FSceneNodeEdit> Edits;
	std::optional<FMat4> ParentInverse;
	for (const auto Handle : FilterSceneSelectionRoots(Scene, InHandles))
	{
		FSceneNodeView View;
		Scene.NodeView(Handle, View);
		if (View.Node->Parent() == ParentId)
		{
			continue;
		}
		if (!ParentInverse)
		{
			ParentInverse = InParent ? ReparentInverse(Parent.World) : Hyperion::Identity();
		}
		auto Node = *View.Node;
		Node.Parent() = ParentId;
		Node.Local() = Multiply(*ParentInverse, View.World);
		ValidateSceneNode(Node);
		if (!IsAffine(InParent ? Multiply(Parent.World, Node.Local()) : Node.Local()))
		{
			throw std::invalid_argument("Reparent produces an invalid world transform");
		}
		Edits.push_back({Handle, std::move(Node)});
	}
	return Edits;
}
} // namespace Hyperion
