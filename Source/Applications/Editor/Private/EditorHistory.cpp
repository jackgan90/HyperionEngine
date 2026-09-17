#include "EditorApplication.h"

namespace Hyperion
{
void FEditorApplication::CommitSettings(FSceneSettings InSettings)
{
	if (HasDrafts())
	{
		throw std::runtime_error("Apply or revert component drafts before changing scene settings");
	}
	if (Scene->GetSettings() == InSettings)
	{
		return;
	}
	FHistoryEntry Entry{{}, {}, {}, Scene->GetSettings(), InSettings, DocumentState, ++NextDocumentState};
	History.reserve(HistoryCursor + 1);
	Scene->SetSettings(std::move(InSettings));
	History.resize(HistoryCursor);
	DocumentState = Entry.AfterState;
	History.push_back(std::move(Entry));
	++HistoryCursor;
	Error.clear();
}

void FEditorApplication::RemapHistoryHandle(FSceneHandle InBefore, FSceneHandle InAfter)
{
	for (auto& Entry : History)
	{
		if (Entry.Handle == InBefore)
		{
			Entry.Handle = InAfter;
		}
		for (auto* Settings : {&Entry.BeforeSettings, &Entry.AfterSettings})
		{
			for (auto* Reference :
			     {&Settings->DefaultCamera, &Settings->MainDirectionalLight, &Settings->EnvironmentLight})
			{
				if (*Reference == InBefore)
				{
					*Reference = InAfter;
				}
			}
		}
	}
	if (PreviewCamera == InBefore)
	{
		PreviewCamera = InAfter;
	}
}

void FEditorApplication::RestoreHistory(std::size_t InIndex, bool bInAfter)
{
	auto& Entry = History.at(InIndex);
	const auto& Node = bInAfter ? Entry.After : Entry.Before;
	if (Entry.Handle.Scene)
	{
		if (!Node)
		{
			if (!Scene->RemoveSubtree(Entry.Handle))
			{
				throw std::runtime_error("Undo target no longer exists");
			}
			if (Selection == Entry.Handle)
			{
				Selection.reset();
			}
		}
		else if (!Entry.Before && bInAfter)
		{
			// Recreated objects receive a fresh generation; later history must follow that exact object.
			const auto Handle = Scene->AddNode(*Node);
			RemapHistoryHandle(Entry.Handle, Handle);
			Selection = Handle;
		}
		else if (!Scene->EditNode(Entry.Handle, *Node, Scene->GetRevision()))
		{
			throw std::runtime_error("History target no longer exists");
		}
	}
	Scene->SetSettings(bInAfter ? Entry.AfterSettings : Entry.BeforeSettings);
}
} // namespace Hyperion
