#pragma once
#include "Hyperion/Gui/Gui.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
// A document presentation item, deliberately separate from handle-based object rows.
struct FOutlinerSceneItem
{
	explicit FOutlinerSceneItem(const FSceneEditDocument& InDocument)
	    : Id("OutlinerScene/" + InDocument.Id()),
	      Label(InDocument.GetState().Path.empty() ? "Untitled Scene"
	                                               : PathToUtf8(PathFromUtf8(InDocument.GetState().Path).stem()))
	{
	}

	bool Draw(FGui& InGui, bool bInExpand = false) const
	{
		InGui.NextRow();
		InGui.NextColumn();
		if (bInExpand)
		{
			InGui.OpenNextTreeItem();
		}
		bool bClicked{};
		return InGui.TreeItem(Id.c_str(), Label.c_str(), false, false, bClicked);
	}

	std::string Id;
	std::string Label;
};
} // namespace Hyperion
