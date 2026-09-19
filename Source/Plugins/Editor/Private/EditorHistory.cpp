#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
void FEditorPlugin::FinishInspectorEdit()
{
	if (Gui && (InspectorInteraction || InspectorTransaction))
	{
		Gui->FinishEditing();
	}
	PendingInspectorEdit.reset();
	InspectorTransaction.reset();
	InspectorInteraction = 0;
}

void FEditorPlugin::RouteHistoryShortcuts(std::vector<FInputEvent>& InEvents)
{
	const bool bAllowHistory =
	    !bOpenDialog && !bSaveDialog && !bDiscardDialog && (!Gui->IsEditingText() || InspectorInteraction != 0);
	std::erase_if(InEvents,
	              [&](const FInputEvent& InEvent)
	              {
		              if (!bAllowHistory || InEvent.Type != EEventType::Key || !InEvent.bDown ||
		                  !(InEvent.Modifiers & 1) || (InEvent.Key != EKey::Z && InEvent.Key != EKey::Y))
		              {
			              return false;
		              }
		              try
		              {
			              if (InEvent.Key == EKey::Y || (InEvent.Modifiers & 2))
			              {
				              Redo();
			              }
			              else
			              {
				              Undo();
			              }
		              }
		              catch (const std::exception& Failure)
		              {
			              Error = Failure.what();
		              }
		              return true;
	              });
}

void FEditorPlugin::CommitSettings(FSceneSettings InSettings)
{
	FinishInspectorEdit();
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

void FEditorPlugin::RemapHistoryHandle(FSceneHandle InBefore, FSceneHandle InAfter)
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

void FEditorPlugin::RestoreHistory(std::size_t InIndex, bool bInAfter)
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
