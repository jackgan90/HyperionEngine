#include "RenderSceneInternal.h"
#include <set>

namespace Hyperion
{
FRenderSceneSnapshot FRenderScene::CollectPrimitives(FRenderView InView,
                                                     std::span<const FRenderPrimitiveHandle> InHandles)
{
	Tasks.Require({EDomain::Render});
	FRenderSceneSnapshot Result;
	Result.View = std::move(InView);
	std::set<std::uint32_t> Seen;
	for (const auto Handle : InHandles)
	{
		const auto It = Entries.find(Handle.Slot);
		if (It == Entries.end() || It->second.Handle != Handle || !Seen.insert(Handle.Slot).second)
		{
			continue;
		}
		const auto& Entry = It->second;
		if (!Entry.Primitive->GetState().bVisible)
		{
			continue;
		}
		std::vector<FRenderItem> Items;
		Entry.Primitive->Collect(Result.View, Items);
		for (std::size_t Index = 0; Index < Items.size(); ++Index)
		{
			auto& Item = Items[Index];
			Item.Primitive = Handle;
			Item.Group = Entry.Group;
			Item.Ordinal = Index;
			Item.Lifetime = Entry.Lifetime;
			Item.EvaluationCache = Entry.EvaluationCache;
			Item.Report = {}; // Auxiliary passes must not replace the normal scene draw diagnostic.
			Result.Items.PushBack(std::move(Item));
		}
	}
	return Result;
}
} // namespace Hyperion
