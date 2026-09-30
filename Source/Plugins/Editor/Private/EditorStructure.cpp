#include "EditorApplication.h"
#include "Hyperion/SceneEditing/SceneAuthoring.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
constexpr const char* ReparentPayload = "Hyperion.SceneNodes.v1";

bool ContainsPoint(FVec4 InBounds, FVec2 InPoint)
{
	return InPoint.X >= InBounds.X && InPoint.X <= InBounds.Z && InPoint.Y >= InBounds.Y && InPoint.Y <= InBounds.W;
}
} // namespace

void FEditorPlugin::BeginReparentGesture(FSceneHandle InHandle, FVec4 InBounds)
{
	const auto Pointer = Gui->PointerState();
	if (!Scene->GetStatus().bReady || Gui->DragPayload() || Pointer.bRightDown || bOpenDialog || bSaveDialog ||
	    bAssetMessage || Transition.PendingRoot || Transition.bDiscardDialog || bPreferencesDialog ||
	    Placement.IsActive())
	{
		return;
	}
	FinishInspectorEdit();
	// A selected row keeps the original group until click versus drag is resolved.
	if (!Selection.Contains(InHandle) && !Pointer.bCtrl && !Pointer.bShift)
	{
		SelectObject(InHandle);
	}
	ReparentGesture = FReparentGesture{std::to_string(++ReparentSerial),
	                                   SceneDocument.Id(),
	                                   Scene->GetRevision(),
	                                   Selection.All(),
	                                   InHandle,
	                                   Pointer.Position,
	                                   InBounds,
	                                   Pointer.bCtrl,
	                                   Pointer.bShift};
}

void FEditorPlugin::CancelReparentGesture()
{
	ReparentGesture.reset();
	ReparentDrop.reset();
	if (const auto Payload = Gui->DragPayload(); Payload && Payload->Type == ReparentPayload)
	{
		Gui->CancelDragDrop();
	}
}

void FEditorPlugin::UpdateReparentGesture(std::span<const FInputEvent> InEvents)
{
	if (!ReparentGesture)
	{
		return;
	}
	const auto Pointer = Gui->PointerState();
	const bool bLostFocus = std::any_of(InEvents.begin(), InEvents.end(),
	                                    [](const FInputEvent& InEvent)
	                                    {
		                                    return InEvent.Type == EEventType::Focus && !InEvent.bDown;
	                                    });
	const auto& Gesture = *ReparentGesture;
	if (bLostFocus || Pointer.bCancel || Pointer.bRightDown || !Pointer.bPositionValid || !bShowOutliner ||
	    Gesture.Document != SceneDocument.Id() || Gesture.Revision != Scene->GetRevision() ||
	    Gesture.Handles != Selection.All() || bOpenDialog || bSaveDialog || bAssetMessage || Transition.PendingRoot ||
	    Transition.bDiscardDialog || bPreferencesDialog || !Scene->GetStatus().bReady)
	{
		CancelReparentGesture();
		return;
	}
	const float DeltaX = Pointer.Position.X - Gesture.Start.X;
	const float DeltaY = Pointer.Position.Y - Gesture.Start.Y;
	const float Threshold = 5 * Gui->ApplicationScale();
	if (!Gesture.bDragging && Pointer.bDown && DeltaX * DeltaX + DeltaY * DeltaY > Threshold * Threshold)
	{
		if (!Selection.Contains(Gesture.Source))
		{
			auto Updated = Selection;
			Updated.Toggle(Gesture.Source);
			SetSelection(std::move(Updated));
			ReparentGesture->Handles = Selection.All();
		}
		ReparentGesture->bDragging = true;
		ViewportClick.reset();
		Camera.Reset();
		Viewport.bCameraDragging = false;
	}
	if (Gesture.bDragging)
	{
		ReparentGesture->bTargetPreview = false;
		const auto Label = Gesture.Handles.size() == 1 ? Scene->FindNode(Gesture.Source)->Name
		                                               : std::to_string(Gesture.Handles.size()) + " selected objects";
		Gui->DragSource(ReparentPayload, Gesture.Token, Label.c_str(), true);
	}
}

void FEditorPlugin::RouteReparentRow(FSceneHandle InHandle, bool bInActivated)
{
	if (Gui->IsItemPressed())
	{
		BeginReparentGesture(InHandle, Gui->LastItemBounds());
	}
	else if (bInActivated && !ReparentGesture && !Gui->PointerState().bReleased)
	{
		// Keyboard activation has no mouse gesture; mouse release is resolved once by FinishReparentGesture.
		ClickOutlinerObject(InHandle, Gui->PointerState().bCtrl, false);
	}
	DrawReparentTarget(InHandle);
}

void FEditorPlugin::DrawReparentTarget(std::optional<FSceneHandle> InParent)
{
	if (!ReparentGesture || !ReparentGesture->bDragging)
	{
		return;
	}
	const auto Drop = Gui->DropTarget(ReparentPayload);
	if (!Drop || Drop->Value != ReparentGesture->Token)
	{
		return;
	}
	ReparentGesture->bTargetPreview = true;
	bool bValid = true;
	std::string Message;
	try
	{
		const auto Edits = SceneDocument.PrepareReparent(ReparentGesture->Handles, InParent);
		Message = Edits.empty() ? "Already under this parent" : "Release to reparent (keep world)";
		FSceneNodeView Parent;
		if (InParent && Scene->GetNodeView(*InParent, Parent) && !Parent.bEffectiveEnabled)
		{
			Message += " - target is disabled; children will be hidden";
		}
	}
	catch (const std::exception& Failure)
	{
		bValid = false;
		Message = Failure.what();
	}
	Gui->DrawDropFeedback(bValid, Message.c_str());
	if (Drop->bDelivery)
	{
		if (bValid)
		{
			ReparentDrop.emplace(InParent);
		}
		else
		{
			Error = Message;
		}
	}
}

void FEditorPlugin::DrawReparentRoot()
{
	if (ReparentGesture && ReparentGesture->bDragging)
	{
		Gui->Selectable("Move to scene root##ReparentRoot", false);
		InspectionBounds["hierarchy/root"] = Gui->LastItemBounds();
		DrawReparentTarget(std::nullopt);
	}
}

void FEditorPlugin::FinishReparentGesture()
{
	if (!ReparentGesture)
	{
		return;
	}
	const auto Pointer = Gui->PointerState();
	if (ReparentDrop)
	{
		const FSceneNodesReparentRequest Request{ReparentGesture->Document, ReparentGesture->Revision,
		                                         ReparentGesture->Handles, *ReparentDrop};
		CancelReparentGesture();
		UpdateDocumentInteraction();
		try
		{
			ReparentSceneNodes(SceneDocument, Request);
			if (Request.Parent)
			{
				ReparentOpenNodes.insert(Scene->FindNode(*Request.Parent)->Id);
			}
			Error.clear();
		}
		catch (const std::exception& Failure)
		{
			Error = Failure.what();
		}
	}
	else if (Pointer.bReleased || !Pointer.bDown)
	{
		const auto Gesture = std::move(*ReparentGesture);
		CancelReparentGesture();
		if (!Gesture.bDragging && ContainsPoint(Gesture.Bounds, Pointer.Position))
		{
			ClickOutlinerObject(Gesture.Source, Gesture.bToggle, Gesture.bRange);
		}
	}
}
} // namespace Hyperion
