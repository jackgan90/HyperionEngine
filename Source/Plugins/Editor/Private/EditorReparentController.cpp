#include "EditorReparentController.h"
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

bool FEditorReparentController::HasGesture() const
{
	return Gesture.has_value();
}

bool FEditorReparentController::IsDragging() const
{
	return Gesture && Gesture->bDragging;
}

const std::optional<FReparentGesture>& FEditorReparentController::GetGesture() const
{
	return Gesture;
}

void FEditorReparentController::Begin(FGui& InGui, FSceneEditDocument& InDocument, IEditorReparentActions& InActions,
                                      FSceneHandle InHandle, FVec4 InBounds, bool bInAllowed)
{
	if (!bInAllowed)
	{
		return;
	}
	const auto Pointer = InGui.PointerState();
	InActions.FinishInspectorEdit();
	// A selected row keeps the original group until click versus drag is resolved.
	if (!InDocument.Selection().Contains(InHandle) && !Pointer.bCtrl && !Pointer.bShift)
	{
		InActions.SelectObject(InHandle);
	}
	Gesture = FReparentGesture{std::to_string(++Serial),
	                           InDocument.Id(),
	                           InDocument.Target().Revision(),
	                           InDocument.Selection().All(),
	                           InHandle,
	                           Pointer.Position,
	                           InBounds,
	                           Pointer.bCtrl,
	                           Pointer.bShift};
}

void FEditorReparentController::Cancel(FGui* InGui)
{
	Gesture.reset();
	Delivery = std::monostate{};
	if (InGui)
	{
		if (const auto Payload = InGui->DragPayload(); Payload && Payload->Type == ReparentPayload)
		{
			InGui->CancelDragDrop();
		}
	}
}

void FEditorReparentController::Reset(FGui* InGui)
{
	Cancel(InGui);
	OpenNodes.clear();
}

bool FEditorReparentController::ConsumeExpansion(const std::string& InNodeId)
{
	return OpenNodes.erase(InNodeId) != 0;
}

void FEditorReparentController::Update(FGui& InGui, FSceneEditDocument& InDocument, IEditorReparentActions& InActions,
                                       std::span<const FInputEvent> InEvents, bool bInAllowed, bool bInOutlinerVisible)
{
	if (!Gesture)
	{
		return;
	}
	const bool bLostFocus = std::any_of(InEvents.begin(), InEvents.end(),
	                                    [](const FInputEvent& InEvent)
	                                    {
		                                    return InEvent.Type == EEventType::Focus && !InEvent.bDown;
	                                    });
	if (bLostFocus || !bInAllowed || !bInOutlinerVisible || Gesture->Document != InDocument.Id() ||
	    Gesture->Revision != InDocument.Target().Revision() || Gesture->Handles != InDocument.Selection().All())
	{
		Cancel(&InGui);
		return;
	}
	const auto Pointer = InGui.PointerState();
	const float DeltaX = Pointer.Position.X - Gesture->Start.X;
	const float DeltaY = Pointer.Position.Y - Gesture->Start.Y;
	const float Threshold = 5 * InGui.ApplicationScale();
	if (!Gesture->bDragging && Pointer.bDown && DeltaX * DeltaX + DeltaY * DeltaY > Threshold * Threshold)
	{
		if (!InDocument.Selection().Contains(Gesture->Source))
		{
			auto Updated = InDocument.Selection();
			Updated.Toggle(Gesture->Source);
			InActions.SetSelection(std::move(Updated));
			Gesture->Handles = InDocument.Selection().All();
		}
		Gesture->bDragging = true;
		InActions.BeginReparentDrag();
	}
	if (Gesture->bDragging)
	{
		Gesture->bTargetPreview = false;
		const auto Label = Gesture->Handles.size() == 1 ? InDocument.RequireNode(Gesture->Source).Name
		                                                : std::to_string(Gesture->Handles.size()) + " selected objects";
		InGui.DragSource(ReparentPayload, Gesture->Token, Label.c_str(), true);
	}
}

FEditorReparentFeedback FEditorReparentController::RouteRow(FGui& InGui, FSceneEditDocument& InDocument,
                                                            IEditorReparentActions& InActions, FSceneHandle InHandle,
                                                            bool bInActivated, bool bInAllowed)
{
	if (InGui.IsItemPressed())
	{
		Begin(InGui, InDocument, InActions, InHandle, InGui.LastItemBounds(), bInAllowed);
	}
	else if (bInActivated && !Gesture && !InGui.PointerState().bReleased)
	{
		// Keyboard activation has no mouse gesture; mouse release is resolved once by Finish.
		InActions.ClickOutlinerObject(InHandle, InGui.PointerState().bCtrl, false);
	}
	return {DrawTarget(InGui, InDocument, InHandle), {}};
}

std::optional<std::string> FEditorReparentController::DrawTarget(FGui& InGui, FSceneEditDocument& InDocument,
                                                                 std::optional<FSceneHandle> InParent)
{
	if (!IsDragging())
	{
		return {};
	}
	const auto Drop = InGui.DropTarget(ReparentPayload);
	if (!Drop || Drop->Value != Gesture->Token)
	{
		return {};
	}
	Gesture->bTargetPreview = true;
	bool bValid = true;
	std::string Message;
	try
	{
		const auto Edits = InDocument.PrepareReparent(Gesture->Handles, InParent);
		Message = Edits.empty() ? "Already under this parent" : "Release to reparent (keep world)";
		FSceneNodeView Parent;
		if (InParent && InDocument.Target().NodeView(*InParent, Parent) && !Parent.bEffectiveEnabled)
		{
			Message += " - target is disabled; children will be hidden";
		}
	}
	catch (const std::exception& Failure)
	{
		bValid = false;
		Message = Failure.what();
	}
	InGui.DrawDropFeedback(bValid, Message.c_str());
	if (Drop->bDelivery)
	{
		if (!bValid)
		{
			return Message;
		}
		if (InParent)
		{
			Delivery = FNodeDelivery{*InParent};
		}
		else
		{
			Delivery = FRootDelivery{};
		}
	}
	return {};
}

FEditorReparentFeedback FEditorReparentController::DrawRoot(FGui& InGui, FSceneEditDocument& InDocument)
{
	if (!IsDragging())
	{
		return {};
	}
	InGui.Selectable("Move to scene root##ReparentRoot", false);
	const auto Bounds = InGui.LastItemBounds();
	return {DrawTarget(InGui, InDocument, std::nullopt), Bounds};
}

std::optional<std::string> FEditorReparentController::Finish(FGui& InGui, FSceneEditDocument& InDocument,
                                                             IEditorReparentActions& InActions)
{
	if (!Gesture)
	{
		return {};
	}
	const auto Pointer = InGui.PointerState();
	if (!std::holds_alternative<std::monostate>(Delivery))
	{
		std::optional<FSceneHandle> Parent;
		if (const auto* Node = std::get_if<FNodeDelivery>(&Delivery))
		{
			Parent = Node->Parent;
		}
		const FSceneNodesReparentRequest Request{Gesture->Document, Gesture->Revision, Gesture->Handles, Parent};
		Cancel(&InGui);
		InActions.UpdateDocumentInteraction();
		try
		{
			ReparentSceneNodes(InDocument, Request);
			if (Request.Parent)
			{
				OpenNodes.insert(InDocument.RequireNode(*Request.Parent).Id);
			}
			return std::string{};
		}
		catch (const std::exception& Failure)
		{
			return std::string(Failure.what());
		}
	}
	if (Pointer.bReleased || !Pointer.bDown)
	{
		const auto Completed = std::move(*Gesture);
		Cancel(&InGui);
		if (!Completed.bDragging && ContainsPoint(Completed.Bounds, Pointer.Position))
		{
			InActions.ClickOutlinerObject(Completed.Source, Completed.bToggle, Completed.bRange);
		}
	}
	return {};
}
} // namespace Hyperion
