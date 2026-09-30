#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
namespace
{
FBounds NodeFramingBounds(const FSceneInstance& InScene, FSceneHandle InHandle, const FSceneNodeView& InView)
{
	if (const auto& Model = InView.Node->Model())
	{
		if (Model->Data)
		{
			const auto Local = SceneModelBounds(SceneModelTransfer(*InView.Node, InView.World, true));
			if (Local.bValid)
			{
				const auto World = TransformBounds(Local, InView.World);
				if (!IsUsable(World))
				{
					throw FSceneEditError("invalid_arguments", "Selected model has invalid framing bounds");
				}
				return World;
			}
		}
		else if (InScene.GetError(InHandle).empty())
		{
			throw FSceneEditError("busy", "Selected model geometry is still loading");
		}
	}
	const FVec3 Center{InView.World.Values[12], InView.World.Values[13], InView.World.Values[14]};
	constexpr FVec3 Extent{.5f, .5f, .5f};
	const FBounds Bounds{Subtract(Center, Extent), Add(Center, Extent), true};
	if (!IsUsable(Bounds))
	{
		throw FSceneEditError("invalid_arguments", "Selected object has invalid framing bounds");
	}
	return Bounds;
}
} // namespace

FBounds SceneSelectionBounds(const FSceneInstance& InScene, std::span<const FSceneHandle> InHandles)
{
	for (const auto Handle : InHandles)
	{
		if (!InScene.FindNode(Handle))
		{
			throw FSceneEditError("stale_handle", "Selected object is no longer in this scene");
		}
	}
	std::vector<FSceneHandle> Pending(InHandles.begin(), InHandles.end());
	std::unordered_set<FSceneHandle, FSceneHandleHash> Visited;
	FBounds Bounds;
	while (!Pending.empty())
	{
		const auto Handle = Pending.back();
		Pending.pop_back();
		if (!Visited.insert(Handle).second)
		{
			continue;
		}
		FSceneNodeView View;
		if (!InScene.GetNodeView(Handle, View))
		{
			throw FSceneEditError("stale_handle", "Selected subtree is no longer in this scene");
		}
		const auto Children = InScene.GetChildren(Handle);
		Pending.insert(Pending.end(), Children.begin(), Children.end());
		if (View.Node->GetKind() == ESceneNodeKind::Group && !Children.empty())
		{
			continue;
		}
		const auto World = NodeFramingBounds(InScene, Handle, View);
		Bounds = IsUsable(Bounds) ? UnionBounds(Bounds, World) : World;
	}
	return Bounds;
}
} // namespace Hyperion
