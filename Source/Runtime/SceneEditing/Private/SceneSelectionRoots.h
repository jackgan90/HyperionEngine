#pragma once
#include "Hyperion/SceneEditing/SceneEditTarget.h"
#include "Hyperion/SceneEditing/SceneSelection.h"

namespace Hyperion
{
// Caller validates handles first when its contract rejects stale members. Selection tolerates removed nodes.
inline std::vector<FSceneHandle> FilterSceneSelectionRoots(const ISceneEditTarget& InTarget,
                                                           std::span<const FSceneHandle> InHandles)
{
	const std::unordered_set<FSceneHandle, FSceneHandleHash> Selected(InHandles.begin(), InHandles.end());
	std::vector<FSceneHandle> Roots;
	for (const auto Handle : InHandles)
	{
		const auto* Node = InTarget.FindNode(Handle);
		bool bCovered = !Node;
		while (Node && !Node->Parent().empty())
		{
			const auto Parent = InTarget.FindHandle(Node->Parent());
			if (Selected.contains(Parent))
			{
				bCovered = true;
				break;
			}
			Node = InTarget.FindNode(Parent);
		}
		if (!bCovered)
		{
			Roots.push_back(Handle);
		}
	}
	return Roots;
}
} // namespace Hyperion
