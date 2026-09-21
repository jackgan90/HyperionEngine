#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
FSceneHandle FEditorPlugin::CommitCreate(FSceneNode InNode)
{
	FinishInspectorEdit();
	do
	{
		InNode.Id = "object-" + std::to_string(++NextDocumentState);
	} while (Scene->FindHandle(InNode.Id).Scene);
	FHistoryEntry Entry{{}, {}, InNode, Scene->GetSettings(), Scene->GetSettings(), DocumentState, NextDocumentState};
	History.reserve(HistoryCursor + 1);
	Entry.Handle = Scene->AddNode(std::move(InNode));
	try
	{
		if (Entry.After->DirectionalLight() && !Entry.BeforeSettings.MainDirectionalLight)
		{
			Entry.AfterSettings.MainDirectionalLight = Entry.Handle;
			Scene->SetSettings(Entry.AfterSettings);
		}
	}
	catch (...)
	{
		Scene->RemoveSubtree(Entry.Handle);
		throw;
	}
	Selection = Entry.Handle;
	bSelectionInitialized = true;
	History.resize(HistoryCursor);
	DocumentState = Entry.AfterState;
	const auto Handle = Entry.Handle;
	History.push_back(std::move(Entry));
	++HistoryCursor;
	Error.clear();
	return Handle;
}

void FEditorPlugin::FinishInspectorEdit()
{
	FinishGizmo();
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
	const bool bAllowHistory = !Placement.IsActive() && !Gui->DragPayload() && !bOpenDialog && !bSaveDialog &&
	                           !bDiscardDialog && !bAssetMessage && !PendingRoot && !bPreferencesDialog &&
	                           (!Gui->IsEditingText() || InspectorInteraction != 0);
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
	RemapHistoryHandles(FEditorHandleMap{{InBefore, InAfter}});
}

void FEditorPlugin::RemapHistoryHandles(const FEditorHandleMap& InMapping)
{
	RemapEditorHistory(History, InMapping);
	RemapEditorHandle(PreviewCamera, InMapping);
	Selection.Remap(InMapping);
}

void FEditorPlugin::RestoreHistory(std::size_t InIndex, bool bInAfter)
{
	auto& Entry = History.at(InIndex);
	const auto& Node = bInAfter ? Entry.After : Entry.Before;
	if (!Entry.Edits.empty())
	{
		std::vector<FSceneNodeEdit> Edits;
		for (const auto& Edit : Entry.Edits)
		{
			Edits.push_back({Edit.Handle, bInAfter ? Edit.After : Edit.Before});
		}
		if (!Scene->EditNodes(std::move(Edits), Scene->GetRevision()))
		{
			throw std::runtime_error("History targets no longer exist");
		}
	}
	else if (!Entry.DeletedSubtree.empty() && !bInAfter)
	{
		RestoreDeletedSubtree(InIndex);
	}
	else if (Entry.Handle.Scene)
	{
		if (!Node)
		{
			const auto Roots = Entry.DeletedRoots.empty() ? std::vector{Entry.Handle} : Entry.DeletedRoots;
			if (!Scene->RemoveSubtrees(Roots))
			{
				throw std::runtime_error("Undo target no longer exists");
			}
			if (!Entry.DeletedSubtree.empty())
			{
				SelectObject(std::nullopt);
			}
			else
			{
				auto Updated = Selection;
				for (const auto Handle : Selection.All())
				{
					if (!Scene->FindNode(Handle))
					{
						Updated.Toggle(Handle);
					}
				}
				SetSelection(std::move(Updated));
			}
			if (PreviewCamera && !Scene->FindNode(*PreviewCamera))
			{
				SetPreviewCamera(std::nullopt);
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
