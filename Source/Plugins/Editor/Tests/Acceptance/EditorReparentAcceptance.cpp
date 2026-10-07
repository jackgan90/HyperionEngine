#include "EditorAcceptanceHarness.h"
#include "Hyperion/SceneEditing/SceneAuthoring.h"
#include <cmath>

namespace Hyperion
{
namespace
{
void RequireReparent(bool bInCondition, const std::string& InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error("Reparent acceptance: " + InMessage);
	}
}

void MoveReparent(std::vector<FInputEvent>& InEvents, FVec2 InPoint)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
}

void ReparentButton(std::vector<FInputEvent>& InEvents, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseButton;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}

FVec2 RowCenter(FVec4 InBounds)
{
	return {(InBounds.X + InBounds.Z) / 2, (InBounds.Y + InBounds.W) / 2};
}
} // namespace

void FEditorAcceptanceHarness::PrepareReparentExercise()
{
	std::vector<FSceneNode> Lighting;
	for (const auto Handle : Editor.Scene->GetNodes())
	{
		const auto& Node = *Editor.Scene->FindNode(Handle);
		if (Node.DirectionalLight() || Node.EnvironmentLight())
		{
			Lighting.push_back(Node);
		}
	}
	Editor.Scene->RemoveSubtrees(Editor.Scene->GetRoots());
	for (unsigned Index = 0; Index < 6; ++Index)
	{
		FSceneNode Node;
		Node.Name = "Reparent " + std::to_string(Index);
		Node.Id = "reparent-" + std::to_string(Index);
		if (Index < 2)
		{
			Node.Model() = FSceneModelComponent{};
			Node.Model()->Asset = Editor.PlacementModels.at("Cube").Asset;
			Node.Local() = Translation({Index == 0 ? -1.f : 1.f, float(Index), 0});
		}
		else if (Index == 2)
		{
			Node.Parent() = "reparent-0";
			Node.Local() = Translation({0, 2, 0});
		}
		else if (Index == 3)
		{
			Node.Local() = ComposeTRS({3, 2, -1}, {0, std::sin(.2f), 0, std::cos(.2f)}, {-2, .5f, 3});
		}
		else if (Index == 4)
		{
			Node.Local() = Scale({1, 0, 1});
		}
		else
		{
			Node.PointLight() = FScenePointLight{};
			Node.Local() = Translation({-2, 2, 0});
		}
		Scenario.ReparentExerciseNodes.push_back(Editor.Scene->AddNode(Node));
		Scenario.ReparentExerciseIds.push_back(Node.Id);
		FSceneNodeView View;
		Editor.Scene->GetNodeView(Scenario.ReparentExerciseNodes.back(), View);
		Scenario.ReparentExerciseWorlds.push_back(View.World);
	}
	for (auto& Node : Lighting)
	{
		Editor.Scene->AddNode(Node);
	}
	Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
	Editor.Viewport.bViewportCameraInitialized = true;
	Editor.bShowLightMarkers = true;
	Editor.ResetDocument();
	Scenario.ReparentDrag.TransitionTo(EReparentDragState::PrepareSelection);
}

void FEditorAcceptanceHarness::ExerciseReparentKeyboard(std::vector<FInputEvent>& InEvents)
{

	if (Scenario.ReparentKeyboard.Is(EReparentKeyboardState::PrepareKeyboard))
	{
		Editor.Filter = "Reparent";
		Scenario.ReparentExerciseHistory = Editor.HistoryCursor;
	}
	else if (Scenario.ReparentKeyboard.Is(EReparentKeyboardState::PressRow) ||
	         Scenario.ReparentKeyboard.Is(EReparentKeyboardState::ReleaseRow))
	{
		MoveReparent(InEvents, RowCenter(Scenario.MultiSelectionRows.at(Scenario.ReparentExerciseIds[0])));
		ReparentButton(InEvents, Scenario.ReparentKeyboard.Is(EReparentKeyboardState::PressRow));
	}
	else if (Scenario.ReparentKeyboard.IsAny({EReparentKeyboardState::PressDown, EReparentKeyboardState::ReleaseDown,
	                                          EReparentKeyboardState::PressSpace,
	                                          EReparentKeyboardState::ReleaseSpace}))
	{
		FInputEvent Event;
		Event.Type = EEventType::Key;
		Event.Key =
		    Scenario.ReparentKeyboard.IsAny({EReparentKeyboardState::PrepareKeyboard, EReparentKeyboardState::PressRow,
		                                     EReparentKeyboardState::ReleaseRow, EReparentKeyboardState::PressDown,
		                                     EReparentKeyboardState::ReleaseDown})
		        ? EKey::Down
		        : EKey::Space;
		Event.bDown = Scenario.ReparentKeyboard.Is(EReparentKeyboardState::PressDown) ||
		              Scenario.ReparentKeyboard.Is(EReparentKeyboardState::PressSpace);
		InEvents.push_back(Event);
	}
	else
	{
		RequireReparent(Editor.Selection == Scenario.ReparentExerciseNodes[1] && Editor.Selection.All().size() == 1,
		                "keyboard activation of filtered Outliner row did not select object: " +
		                    (Editor.Selection ? Editor.Scene->FindNode(*Editor.Selection)->Id : "none") +
		                    " gesture=" + std::to_string(Editor.Reparent.GetGesture().has_value()));
		RequireReparent(!Editor.Reparent.GetGesture() && !Editor.Gui->DragPayload() &&
		                    Editor.HistoryCursor == Scenario.ReparentExerciseHistory,
		                "keyboard selection created a drag or history entry");
	}
	switch (Scenario.ReparentKeyboard.GetState())
	{
		case EReparentKeyboardState::PrepareKeyboard:
			Scenario.ReparentKeyboard.TransitionTo(EReparentKeyboardState::PressRow);
			break;
		case EReparentKeyboardState::PressRow:
			Scenario.ReparentKeyboard.TransitionTo(EReparentKeyboardState::ReleaseRow);
			break;
		case EReparentKeyboardState::ReleaseRow:
			Scenario.ReparentKeyboard.TransitionTo(EReparentKeyboardState::PressDown);
			break;
		case EReparentKeyboardState::PressDown:
			Scenario.ReparentKeyboard.TransitionTo(EReparentKeyboardState::ReleaseDown);
			break;
		case EReparentKeyboardState::ReleaseDown:
			Scenario.ReparentKeyboard.TransitionTo(EReparentKeyboardState::PressSpace);
			break;
		case EReparentKeyboardState::PressSpace:
			Scenario.ReparentKeyboard.TransitionTo(EReparentKeyboardState::ReleaseSpace);
			break;
		case EReparentKeyboardState::ReleaseSpace:
			Scenario.ReparentKeyboard.TransitionTo(EReparentKeyboardState::VerifyKeyboardSelection);
			break;
		case EReparentKeyboardState::VerifyKeyboardSelection:
			Scenario.ReparentKeyboard.TransitionTo(EReparentKeyboardState::Complete);
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseReparentSelection(std::vector<FInputEvent>& InEvents)
{
	const bool bLight = DescribeReparentSelectionContext(Scenario.ReparentSelection.GetState()).CaseIndex == 1;
	const auto Phase = DescribeReparentSelectionContext(Scenario.ReparentSelection.GetState()).Action;
	const auto Handle = Scenario.ReparentExerciseNodes[bLight ? 5 : 0];
	const auto Screen = ProjectViewportPoint(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportRegion.Bounds,
	                                         bLight ? FVec3{-2, 2, 0} : FVec3{-1.3f, -.3f, .5f});
	RequireReparent(Screen.has_value(), "selection point is outside viewport");
	const FVec2 Point{Screen->X, Screen->Y};
	if (Phase == EReparentSelectionAction::PrepareSelection)
	{
		Editor.SelectObject(std::nullopt);
		Scenario.ReparentExerciseHistory = Editor.HistoryCursor;
		Editor.Filter.clear();
	}
	else if (Phase == EReparentSelectionAction::PressViewport || Phase == EReparentSelectionAction::PressViewportDrag)
	{
		if (Phase == EReparentSelectionAction::PressViewportDrag && !bLight)
		{
			RequireReparent(Editor.Gizmo.HitTest(Point) == ETransformGizmoHandle::None,
			                "mesh drag must exercise viewport input outside gizmo handles");
		}
		MoveReparent(InEvents, Point);
		ReparentButton(InEvents, true);
	}
	else if (Phase == EReparentSelectionAction::ReleaseViewport)
	{
		ReparentButton(InEvents, false);
	}
	else if (Phase == EReparentSelectionAction::VerifyViewportSelection)
	{
		RequireReparent(Editor.Selection == Handle && Editor.Selection.All().size() == 1,
		                "viewport click must select the same object used by Outliner");
	}
	else if (Phase == EReparentSelectionAction::MoveViewportDrag || Phase == EReparentSelectionAction::MoveToOutliner ||
	         Phase == EReparentSelectionAction::CancelViewportDrag)
	{
		RequireReparent(!Editor.Reparent.GetGesture() && !Editor.Gui->DragPayload(),
		                "viewport must not start hierarchy drag");
		if (Phase == EReparentSelectionAction::MoveViewportDrag)
		{
			MoveReparent(InEvents, {Point.X + 20, Point.Y});
		}
		else if (Phase == EReparentSelectionAction::MoveToOutliner)
		{
			MoveReparent(InEvents, RowCenter(Scenario.MultiSelectionRows.at(Scenario.ReparentExerciseIds[3])));
		}
		else
		{
			RequireReparent(Editor.Scene->FindNode(Handle)->Parent().empty(), "viewport drag changed parent");
			FInputEvent Event;
			Event.Type = EEventType::Key;
			Event.Key = EKey::Escape;
			Event.bDown = true;
			InEvents.push_back(Event);
		}
	}
	else if (Phase == EReparentSelectionAction::ReleaseViewportDrag)
	{
		ReparentButton(InEvents, false);
		FInputEvent Event;
		Event.Type = EEventType::Key;
		Event.Key = EKey::Escape;
		InEvents.push_back(Event);
	}
	else if (Phase == EReparentSelectionAction::VerifyCancelledDrag)
	{
		RequireReparent(Editor.Selection == Handle && Editor.HistoryCursor == Scenario.ReparentExerciseHistory,
		                "selection or history changed after cancelled viewport gesture");
		MoveReparent(InEvents, RowCenter(Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(Handle)->Id)));
	}
	else if (Phase == EReparentSelectionAction::PressOutliner)
	{
		// Let mouse/key release events drain before starting the next independent gesture.
		ReparentButton(InEvents, true);
	}
	else if (Phase == EReparentSelectionAction::StartOutlinerDrag)
	{
		RequireReparent(Editor.Reparent.GetGesture().has_value(), "Outliner did not use viewport selection");
		MoveReparent(InEvents, {Editor.Reparent.GetGesture()->Start.X + 20, Editor.Reparent.GetGesture()->Start.Y});
	}
	else if (Phase == EReparentSelectionAction::MoveOutlinerTarget)
	{
		MoveReparent(InEvents, RowCenter(Scenario.MultiSelectionRows.at(Scenario.ReparentExerciseIds[3])));
	}
	else if (Phase == EReparentSelectionAction::ReleaseOutlinerDrag)
	{
		RequireReparent(Editor.Reparent.GetGesture() && Editor.Reparent.GetGesture()->bTargetPreview,
		                "Outliner target did not preview");
		ReparentButton(InEvents, false);
	}
	else
	{
		RequireReparent(Editor.Scene->FindNode(Handle)->Parent() == Scenario.ReparentExerciseIds[3] &&
		                    Editor.HistoryCursor == Scenario.ReparentExerciseHistory + 1 && Editor.Selection == Handle,
		                "Outliner reparent after viewport selection failed");
		Editor.Undo();
		RequireReparent(Editor.Scene->FindNode(Handle)->Parent().empty(), "selection exercise undo failed");
	}
	switch (Scenario.ReparentSelection.GetState())
	{
		case EReparentSelectionState::MeshPrepareSelection:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshPressViewport);
			break;
		case EReparentSelectionState::MeshPressViewport:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshReleaseViewport);
			break;
		case EReparentSelectionState::MeshReleaseViewport:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshVerifyViewportSelection);
			break;
		case EReparentSelectionState::MeshVerifyViewportSelection:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshPressViewportDrag);
			break;
		case EReparentSelectionState::MeshPressViewportDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshMoveViewportDrag);
			break;
		case EReparentSelectionState::MeshMoveViewportDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshMoveToOutliner);
			break;
		case EReparentSelectionState::MeshMoveToOutliner:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshCancelViewportDrag);
			break;
		case EReparentSelectionState::MeshCancelViewportDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshReleaseViewportDrag);
			break;
		case EReparentSelectionState::MeshReleaseViewportDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshVerifyCancelledDrag);
			break;
		case EReparentSelectionState::MeshVerifyCancelledDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshPressOutliner);
			break;
		case EReparentSelectionState::MeshPressOutliner:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshStartOutlinerDrag);
			break;
		case EReparentSelectionState::MeshStartOutlinerDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshMoveOutlinerTarget);
			break;
		case EReparentSelectionState::MeshMoveOutlinerTarget:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshReleaseOutlinerDrag);
			break;
		case EReparentSelectionState::MeshReleaseOutlinerDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::MeshVerifyReparent);
			break;
		case EReparentSelectionState::MeshVerifyReparent:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightPrepareSelection);
			break;
		case EReparentSelectionState::LightPrepareSelection:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightPressViewport);
			break;
		case EReparentSelectionState::LightPressViewport:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightReleaseViewport);
			break;
		case EReparentSelectionState::LightReleaseViewport:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightVerifyViewportSelection);
			break;
		case EReparentSelectionState::LightVerifyViewportSelection:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightPressViewportDrag);
			break;
		case EReparentSelectionState::LightPressViewportDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightMoveViewportDrag);
			break;
		case EReparentSelectionState::LightMoveViewportDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightMoveToOutliner);
			break;
		case EReparentSelectionState::LightMoveToOutliner:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightCancelViewportDrag);
			break;
		case EReparentSelectionState::LightCancelViewportDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightReleaseViewportDrag);
			break;
		case EReparentSelectionState::LightReleaseViewportDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightVerifyCancelledDrag);
			break;
		case EReparentSelectionState::LightVerifyCancelledDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightPressOutliner);
			break;
		case EReparentSelectionState::LightPressOutliner:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightStartOutlinerDrag);
			break;
		case EReparentSelectionState::LightStartOutlinerDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightMoveOutlinerTarget);
			break;
		case EReparentSelectionState::LightMoveOutlinerTarget:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightReleaseOutlinerDrag);
			break;
		case EReparentSelectionState::LightReleaseOutlinerDrag:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::LightVerifyReparent);
			break;
		case EReparentSelectionState::LightVerifyReparent:
			Scenario.ReparentSelection.TransitionTo(EReparentSelectionState::Complete);
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseReparentInterruption(std::vector<FInputEvent>& InEvents)
{
	const auto Case = Scenario.ReparentExerciseCase;
	RequireReparent(Case == 7 || (Editor.Reparent.GetGesture() && Editor.Reparent.GetGesture()->bTargetPreview),
	                "target did not preview case " + std::to_string(Case));
	if (Case == 5 || Case == 6 || Case == 11)
	{
		FInputEvent Event;
		Event.Type = Case == 5 ? EEventType::Key : Case == 6 ? EEventType::Focus : EEventType::MouseButton;
		Event.Key = EKey::Escape;
		Event.Button = InputButtons::Right;
		Event.bDown = Case != 6;
		InEvents.push_back(Event);
	}
	if (Case == 8)
	{
		const auto B = Scenario.ReparentExerciseNodes[1];
		auto Node = *Editor.Scene->FindNode(B);
		Node.Name += " changed";
		Editor.Scene->EditNode(B, Node, Editor.Scene->GetRevision());
		Scenario.ReparentExerciseRevision = Editor.Scene->GetRevision();
	}
	if (Case == 12)
	{
		Editor.SceneDocument.Invalidate();
	}
	if (Case == 13)
	{
		Editor.ClickObject(Scenario.ReparentExerciseNodes[2], true);
	}
}

void FEditorAcceptanceHarness::ExerciseReparentDrag(std::vector<FInputEvent>& InEvents)
{
	const auto A = Scenario.ReparentExerciseNodes[0];
	const auto B = Scenario.ReparentExerciseNodes[1];
	const auto Child = Scenario.ReparentExerciseNodes[2];
	const unsigned Case = Scenario.ReparentExerciseCase;
	if (Scenario.ReparentDrag.Is(EReparentDragState::PrepareSelection))
	{
		FEditorSelection Selected;
		Selected.Toggle(A);
		Selected.Toggle(Child);
		Selected.Toggle(B);
		if (Case == 9)
		{
			Selected = B;
		}
		if (Case == 10)
		{
			Selected = Scenario.ReparentExerciseNodes[5];
			Selected.Toggle(B);
		}
		Editor.SetSelection(std::move(Selected));
		Editor.Filter = Case == 1 || Case == 9 ? "Reparent" : "";
		Scenario.ReparentExerciseRevision = Editor.Scene->GetRevision();
		Scenario.ReparentExerciseHistory = Editor.HistoryCursor;
		Editor.Error.clear();
	}
	else if (Scenario.ReparentDrag.Is(EReparentDragState::PressSource))
	{
		Scenario.ReparentSceneBounds = Scenario.InspectionBounds.at("hierarchy/root");
		Scenario.ReparentRowBounds = Scenario.MultiSelectionRows;
		const auto Point = RowCenter(Scenario.MultiSelectionRows.at(Scenario.ReparentExerciseIds[Case == 9    ? 1
		                                                                                         : Case == 10 ? 5
		                                                                                                      : 0]));
		MoveReparent(InEvents, Point);
		ReparentButton(InEvents, true);
	}
	else if (Scenario.ReparentDrag.Is(EReparentDragState::StartDrag))
	{
		RequireReparent(Editor.Reparent.GetGesture().has_value(),
		                "press did not prepare source case " + std::to_string(Case));
		MoveReparent(InEvents, {Editor.Reparent.GetGesture()->Start.X + 20, Editor.Reparent.GetGesture()->Start.Y});
	}
	else if (Scenario.ReparentDrag.Is(EReparentDragState::PreviewTarget))
	{
		RequireReparent(Editor.Reparent.GetGesture() && Editor.Reparent.GetGesture()->bDragging, "drag did not start");
		const auto SameBounds = [](FVec4 InA, FVec4 InB)
		{
			return InA.X == InB.X && InA.Y == InB.Y && InA.Z == InB.Z && InA.W == InB.W;
		};
		RequireReparent(SameBounds(Scenario.ReparentSceneBounds, Scenario.InspectionBounds.at("hierarchy/root")),
		                "Scene container moved when dragging started");
		for (const auto& [Id, Bounds] : Scenario.ReparentRowBounds)
		{
			RequireReparent(SameBounds(Bounds, Scenario.MultiSelectionRows.at(Id)),
			                "Outliner row moved when dragging started: " + Id);
		}
		RequireReparent(Editor.OutlinerRows.size() <= Editor.Scene->GetStatus().Nodes &&
		                    std::all_of(Editor.OutlinerRows.begin(), Editor.OutlinerRows.end(),
		                                [&](FSceneHandle InHandle)
		                                {
			                                return Editor.Scene->FindNode(InHandle) != nullptr;
		                                }),
		                "Scene container entered logical object rows");
		RequireReparent(Editor.Selection.All().size() == (Case == 9    ? 1u
		                                                  : Case == 10 ? 2u
		                                                               : 3u),
		                "drag collapsed the selection");
		const auto Bounds = Case == 1 || Case == 2
		                        ? Scenario.InspectionBounds.at("hierarchy/root")
		                        : Scenario.MultiSelectionRows.at(Scenario.ReparentExerciseIds[Case == 3   ? 2
		                                                                                      : Case == 4 ? 4
		                                                                                                  : 3]);
		MoveReparent(InEvents, Case == 7 ? FVec2{5, 5} : RowCenter(Bounds));
	}
	else if (Scenario.ReparentDrag.Is(EReparentDragState::InterruptGesture))
	{
		ExerciseReparentInterruption(InEvents);
	}
	else if (Scenario.ReparentDrag.Is(EReparentDragState::ReleasePointer))
	{
		ReparentButton(InEvents, false);
	}
	else if (Scenario.ReparentDrag.Is(EReparentDragState::ReleaseInterruption))
	{
		FInputEvent Event;
		Event.Type = Case == 6 ? EEventType::Focus : EEventType::Key;
		Event.Key = EKey::Escape;
		Event.bDown = Case == 6;
		if (Case == 11)
		{
			Event.Type = EEventType::MouseButton;
			Event.Button = InputButtons::Right;
		}
		InEvents.push_back(Event);
	}
	else
	{
		VerifyReparentExercise();
		++Scenario.ReparentExerciseCase;
		Scenario.ReparentDrag.TransitionTo(EReparentDragState::PrepareSelection);
		return;
	}
	switch (Scenario.ReparentDrag.GetState())
	{
		case EReparentDragState::PrepareSelection:
			Scenario.ReparentDrag.TransitionTo(EReparentDragState::PressSource);
			break;
		case EReparentDragState::PressSource:
			Scenario.ReparentDrag.TransitionTo(EReparentDragState::StartDrag);
			break;
		case EReparentDragState::StartDrag:
			Scenario.ReparentDrag.TransitionTo(EReparentDragState::PreviewTarget);
			break;
		case EReparentDragState::PreviewTarget:
			Scenario.ReparentDrag.TransitionTo(EReparentDragState::InterruptGesture);
			break;
		case EReparentDragState::InterruptGesture:
			Scenario.ReparentDrag.TransitionTo(EReparentDragState::ReleasePointer);
			break;
		case EReparentDragState::ReleasePointer:
			Scenario.ReparentDrag.TransitionTo(EReparentDragState::ReleaseInterruption);
			break;
		case EReparentDragState::ReleaseInterruption:
			Scenario.ReparentDrag.TransitionTo(EReparentDragState::VerifyGesture);
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::VerifyReparentExercise()
{
	const auto Case = Scenario.ReparentExerciseCase;
	const bool bChanged = Case == 0 || Case == 1 || Case == 9 || Case == 10;
	RequireReparent(!Editor.Reparent.GetGesture() && !Editor.Gui->DragPayload(),
	                "gesture survived delivery/cancellation");
	RequireReparent(Editor.HistoryCursor == Scenario.ReparentExerciseHistory + (bChanged ? 1 : 0),
	                "unexpected history case " + std::to_string(Case) + " actual " +
	                    std::to_string(Editor.HistoryCursor) + ": " + Editor.Error);
	RequireReparent(Editor.Scene->GetRevision() == Scenario.ReparentExerciseRevision + (bChanged ? 1 : 0),
	                "unexpected revision");
	RequireReparent(Editor.Scene->FindNode(Scenario.ReparentExerciseNodes[2])->Parent() ==
	                    Scenario.ReparentExerciseIds[0],
	                "child was flattened");
	const auto Parent = Case == 0 || Case >= 9 ? Scenario.ReparentExerciseIds[3] : std::string{};
	RequireReparent(Editor.Scene->FindNode(Scenario.ReparentExerciseNodes[1])->Parent() == Parent, "incorrect parent");
	for (std::size_t Index = 0; Index < Scenario.ReparentExerciseNodes.size(); ++Index)
	{
		FSceneNodeView View;
		Editor.Scene->GetNodeView(Scenario.ReparentExerciseNodes[Index], View);
		for (std::size_t Element = 0; Element < View.World.Values.size(); ++Element)
		{
			RequireReparent(
			    std::abs(View.World.Values[Element] - Scenario.ReparentExerciseWorlds[Index].Values[Element]) < .0001f,
			    "world transform changed");
		}
	}
	if (bChanged)
	{
		const auto Selected = Editor.Selection;
		Editor.Undo();
		RequireReparent(Editor.HistoryCursor == Scenario.ReparentExerciseHistory && Editor.Selection == Selected,
		                "undo selection/history");
		Editor.Redo();
		RequireReparent(Editor.Selection == Selected &&
		                    Editor.Scene->FindNode(Scenario.ReparentExerciseNodes[1])->Parent() == Parent,
		                "redo parent/selection");
	}
}

void FEditorAcceptanceHarness::ExerciseReparent(std::vector<FInputEvent>& InEvents)
{
	if (Editor.FrameCount < 12 || !Editor.Scene->GetStatus().bReady || !Editor.Viewport.bViewportVisible ||
	    Editor.GetPlacementPreparation(*Editor.PlacementRegistry.Find("Cube")).State !=
	        EPlacementPreparationState::Ready)
	{
		return;
	}
	if (Scenario.ReparentDrag.Is(EReparentDragState::PrepareFixtures))
	{
		PrepareReparentExercise();
		return;
	}
	if (Scenario.ReparentExerciseCase < 14)
	{
		if (Scenario.ReparentKeyboard.IsAny({EReparentKeyboardState::PrepareKeyboard, EReparentKeyboardState::PressRow,
		                                     EReparentKeyboardState::ReleaseRow, EReparentKeyboardState::PressDown,
		                                     EReparentKeyboardState::ReleaseDown, EReparentKeyboardState::PressSpace,
		                                     EReparentKeyboardState::ReleaseSpace,
		                                     EReparentKeyboardState::VerifyKeyboardSelection}))
		{
			ExerciseReparentKeyboard(InEvents);
			return;
		}
		if ((IsReparentSelectionState(Scenario.ReparentSelection.GetState())))
		{
			ExerciseReparentSelection(InEvents);
			return;
		}
		ExerciseReparentDrag(InEvents);
		return;
	}
	if (Scenario.ReparentDocument.Is(EReparentDocumentState::SaveScene))
	{
		Editor.SaveScene(Editor.Options.ExerciseReparent.generic_string());
		Scenario.ReparentDocument.TransitionTo(EReparentDocumentState::AwaitSaveAndReopen);
	}
	else if (Scenario.ReparentDocument.Is(EReparentDocumentState::AwaitSaveAndReopen) && !Editor.PendingSave)
	{
		RequireReparent(!Editor.IsDirty(), "save failed: " + Editor.Error);
		Editor.OpenScene(Editor.Options.ExerciseReparent.generic_string());
		Scenario.ReparentDocument.TransitionTo(EReparentDocumentState::VerifyReload);
	}
	else if (Scenario.ReparentDocument.IsAny({EReparentDocumentState::PressReplacementSource,
	                                          EReparentDocumentState::StartReplacementDrag,
	                                          EReparentDocumentState::ReplaceSceneDuringDrag}))
	{
		ExerciseReparentReplacement(InEvents);
	}
	else if (Scenario.ReparentDocument.Is(EReparentDocumentState::VerifyReload) ||
	         Scenario.ReparentDocument.Is(EReparentDocumentState::VerifyReplacement))
	{
		for (std::size_t Index = 0; Index < Scenario.ReparentExerciseIds.size(); ++Index)
		{
			const auto Handle = Editor.Scene->FindHandle(Scenario.ReparentExerciseIds[Index]);
			FSceneNodeView View;
			RequireReparent(Editor.Scene->GetNodeView(Handle, View), "saved node missing");
			const auto ExpectedParent = Index == 1 || Index == 5 ? Scenario.ReparentExerciseIds[3]
			                            : Index == 2             ? Scenario.ReparentExerciseIds[0]
			                                                     : "";
			RequireReparent(View.Node->Parent() == ExpectedParent, "saved parent changed");
			for (std::size_t Element = 0; Element < View.World.Values.size(); ++Element)
			{
				RequireReparent(std::abs(View.World.Values[Element] -
				                         Scenario.ReparentExerciseWorlds[Index].Values[Element]) < .0001f,
				                "saved world changed");
			}
		}
		if (Scenario.ReparentDocument.Is(EReparentDocumentState::VerifyReload))
		{
			Scenario.ReparentDocument.TransitionTo(EReparentDocumentState::PressReplacementSource);
		}
		else
		{
			Scenario.bReparentVerified = true;
		}
	}
}

void FEditorAcceptanceHarness::ExerciseReparentReplacement(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.ReparentDocument.Is(EReparentDocumentState::PressReplacementSource))
	{
		const auto Handle = Editor.Scene->FindHandle(Scenario.ReparentExerciseIds[0]);
		const auto Name = Editor.Scene->FindNode(Handle)->Name + " replacement probe";
		SetSceneMetadata(Editor.SceneDocument,
		                 {Editor.SceneDocument.Id(), Editor.Scene->GetRevision(), {{Handle, Name, std::nullopt}}});
		RequireReparent(Editor.HistoryCursor > 0 && Editor.IsDirty(), "replacement fixture needs real history");
		Editor.SetSelection(FSceneSelection(Handle));
		MoveReparent(InEvents, RowCenter(Scenario.MultiSelectionRows.at(Scenario.ReparentExerciseIds[0])));
		ReparentButton(InEvents, true);
	}
	else if (Scenario.ReparentDocument.Is(EReparentDocumentState::StartReplacementDrag))
	{
		RequireReparent(Editor.Reparent.HasGesture(), "replacement fixture did not prepare a gesture");
		const auto Start = Editor.Reparent.GetGesture()->Start;
		MoveReparent(InEvents, {Start.X + 20, Start.Y});
	}
	else
	{
		RequireReparent(Editor.Reparent.IsDragging() && Editor.Gui->DragPayload(),
		                "replacement fixture did not start a drag");
		const auto History = Editor.HistoryCursor;
		const auto HistorySize = Editor.History.size();
		const auto Document = Editor.SceneDocument.Id();
		bool bRejected = false;
		try
		{
			Editor.LoadSceneDocument("/ReparentMissing/Invalid.hasset", true);
		}
		catch (const std::exception&)
		{
			bRejected = true;
		}
		RequireReparent(bRejected, "invalid scene mount did not fail synchronously");
		RequireReparent(!Editor.Reparent.HasGesture() && !Editor.Gui->DragPayload(),
		                "failed scene replacement retained the old gesture/payload");
		RequireReparent(Editor.HistoryCursor == History && Editor.History.size() == HistorySize &&
		                    Editor.SceneDocument.Id() == Document && Editor.IsDirty(),
		                "failed load cleared domain history prematurely");
		Editor.LoadSceneDocument(Editor.Options.ExerciseReparent.generic_string(), true);
		ReparentButton(InEvents, false);
	}
	switch (Scenario.ReparentDocument.GetState())
	{
		case EReparentDocumentState::PressReplacementSource:
			Scenario.ReparentDocument.TransitionTo(EReparentDocumentState::StartReplacementDrag);
			break;
		case EReparentDocumentState::StartReplacementDrag:
			Scenario.ReparentDocument.TransitionTo(EReparentDocumentState::ReplaceSceneDuringDrag);
			break;
		case EReparentDocumentState::ReplaceSceneDuringDrag:
			Scenario.ReparentDocument.TransitionTo(EReparentDocumentState::VerifyReplacement);
			break;
		default:
			break;
	}
}
} // namespace Hyperion
