#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
FSceneHandle FSceneEditDocument::CommitDuplicate(FSceneHandle InHandle)
{
	if (bHistory)
	{
		throw FSceneEditError("unavailable", "Resource-preserving duplication has no history adapter yet");
	}
	const auto Handle = Target().DuplicateNode(InHandle);
	if (Handle.Scene)
	{
		State.State = ++State.NextState;
		ReplaceSelection(FSceneSelection(std::optional{Handle}));
	}
	return Handle;
}

void FSceneEditDocument::CommitRemoveKeepChildren(FSceneHandle InHandle)
{
	if (bHistory)
	{
		throw FSceneEditError("unavailable", "Remove with preserved children has no history adapter yet");
	}
	if (Target().RemoveNodeKeepChildren(InHandle))
	{
		State.State = ++State.NextState;
		if (Selected.Contains(InHandle))
		{
			auto Updated = Selected;
			Updated.Toggle(InHandle);
			ReplaceSelection(std::move(Updated));
		}
	}
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
