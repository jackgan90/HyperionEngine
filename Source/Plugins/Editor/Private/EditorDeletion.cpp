#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
void FEditorPlugin::RouteDeleteShortcut(std::span<const FInputEvent> InEvents)
{
	if (!Selection || !Scene->GetStatus().bReady || Gui->IsEditingText() || Gui->DragPayload() ||
	    Placement.IsActive() || Gizmo.IsDragging() || bCameraDragging || bOpenDialog || bSaveDialog || bDiscardDialog ||
	    Gui->PointerState().bCancel)
	{
		return;
	}
	const bool bDelete = std::any_of(InEvents.begin(), InEvents.end(),
	                                 [](const FInputEvent& InEvent)
	                                 {
		                                 return InEvent.Type == EEventType::Key && InEvent.Key == EKey::Delete &&
		                                        InEvent.bDown && !InEvent.bRepeat && !InEvent.Modifiers;
	                                 });
	if (bDelete)
	{
		try
		{
			CommitDelete();
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
}

void FEditorPlugin::CommitDelete()
{
	FinishInspectorEdit();
	if (!Selection || !Scene->FindNode(*Selection))
	{
		return;
	}
	FHistoryEntry Entry{*Selection, {}, {}, Scene->GetSettings(), {}, DocumentState, ++NextDocumentState};
	std::vector<FSceneHandle> Pending{*Selection};
	for (std::size_t Index = 0; Index < Pending.size(); ++Index)
	{
		const auto Handle = Pending[Index];
		Entry.DeletedSubtree.emplace_back(Handle, *Scene->FindNode(Handle));
		const auto Children = Scene->GetChildren(Handle);
		Pending.insert(Pending.end(), Children.begin(), Children.end());
	}
	History.reserve(HistoryCursor + 1);
	if (!Scene->RemoveSubtree(Entry.Handle))
	{
		throw std::runtime_error("Delete target no longer exists");
	}
	Entry.AfterSettings = Scene->GetSettings();
	SelectObject(std::nullopt);
	ViewportClick.reset();
	if (PreviewCamera && !Scene->FindNode(*PreviewCamera))
	{
		SetPreviewCamera(std::nullopt);
	}
	History.resize(HistoryCursor);
	DocumentState = Entry.AfterState;
	History.push_back(std::move(Entry));
	++HistoryCursor;
	Error.clear();
}

void FEditorPlugin::RestoreDeletedSubtree(std::size_t InIndex)
{
	auto& Entry = History.at(InIndex);
	std::vector<FSceneHandle> Restored;
	Restored.reserve(Entry.DeletedSubtree.size());
	try
	{
		// Parents precede children, whose serialized parent IDs stay stable across generations.
		for (const auto& [Handle, Node] : Entry.DeletedSubtree)
		{
			Restored.push_back(Scene->AddNode(Node));
		}
	}
	catch (...)
	{
		if (!Restored.empty())
		{
			Scene->RemoveSubtree(Restored.front());
		}
		throw;
	}
	for (std::size_t Index = 0; Index < Restored.size(); ++Index)
	{
		RemapHistoryHandle(Entry.DeletedSubtree[Index].first, Restored[Index]);
	}
	SelectObject(Restored.front());
}
} // namespace Hyperion
