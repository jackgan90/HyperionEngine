#include "EditorApplication.h"
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

void FEditorPlugin::PrepareReparentExercise()
{
	std::vector<FSceneNode> Lighting;
	for (const auto Handle : Scene->GetNodes())
	{
		const auto& Node = *Scene->FindNode(Handle);
		if (Node.DirectionalLight() || Node.EnvironmentLight())
		{
			Lighting.push_back(Node);
		}
	}
	Scene->RemoveSubtrees(Scene->GetRoots());
	for (unsigned Index = 0; Index < 6; ++Index)
	{
		FSceneNode Node;
		Node.Name = "Reparent " + std::to_string(Index);
		Node.Id = "reparent-" + std::to_string(Index);
		if (Index < 2)
		{
			Node.Model() = FSceneModelComponent{};
			Node.Model()->Asset = PlacementModels.at("Cube").Asset;
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
		Acceptance.ReparentExerciseNodes.push_back(Scene->AddNode(Node));
		Acceptance.ReparentExerciseIds.push_back(Node.Id);
		FSceneNodeView View;
		Scene->GetNodeView(Acceptance.ReparentExerciseNodes.back(), View);
		Acceptance.ReparentExerciseWorlds.push_back(View.World);
	}
	for (auto& Node : Lighting)
	{
		Scene->AddNode(Node);
	}
	Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
	Viewport.bViewportCameraInitialized = true;
	bShowLightMarkers = true;
	ResetDocument();
	Acceptance.ReparentExerciseStep = 1;
}

void FEditorPlugin::ExerciseReparentKeyboard(std::vector<FInputEvent>& InEvents)
{
	const auto Step = Acceptance.ReparentKeyboardStep;
	if (Step == 0)
	{
		Filter = "Reparent";
		Acceptance.ReparentExerciseHistory = HistoryCursor;
	}
	else if (Step == 1 || Step == 2)
	{
		MoveReparent(InEvents, RowCenter(Acceptance.MultiSelectionRows.at(Acceptance.ReparentExerciseIds[0])));
		ReparentButton(InEvents, Step == 1);
	}
	else if (Step >= 3 && Step <= 6)
	{
		FInputEvent Event;
		Event.Type = EEventType::Key;
		Event.Key = Step <= 4 ? EKey::Down : EKey::Space;
		Event.bDown = Step == 3 || Step == 5;
		InEvents.push_back(Event);
	}
	else
	{
		RequireReparent(Selection == Acceptance.ReparentExerciseNodes[1] && Selection.All().size() == 1,
		                "keyboard activation of filtered Outliner row did not select object: " +
		                    (Selection ? Scene->FindNode(*Selection)->Id : "none") +
		                    " gesture=" + std::to_string(ReparentGesture.has_value()));
		RequireReparent(!ReparentGesture && !Gui->DragPayload() && HistoryCursor == Acceptance.ReparentExerciseHistory,
		                "keyboard selection created a drag or history entry");
	}
	++Acceptance.ReparentKeyboardStep;
}

void FEditorPlugin::ExerciseReparentSelection(std::vector<FInputEvent>& InEvents)
{
	const bool bLight = Acceptance.ReparentSelectionStep >= 15;
	const unsigned Phase = Acceptance.ReparentSelectionStep % 15;
	const auto Handle = Acceptance.ReparentExerciseNodes[bLight ? 5 : 0];
	const auto Screen = ProjectViewportPoint(Viewport.ViewCamera, Viewport.ViewportRegion.Bounds,
	                                         bLight ? FVec3{-2, 2, 0} : FVec3{-1.3f, -.3f, .5f});
	RequireReparent(Screen.has_value(), "selection point is outside viewport");
	const FVec2 Point{Screen->X, Screen->Y};
	if (Phase == 0)
	{
		SelectObject(std::nullopt);
		Acceptance.ReparentExerciseHistory = HistoryCursor;
		Filter.clear();
	}
	else if (Phase == 1 || Phase == 4)
	{
		if (Phase == 4 && !bLight)
		{
			RequireReparent(Gizmo.HitTest(Point) == ETransformGizmoHandle::None,
			                "mesh drag must exercise viewport input outside gizmo handles");
		}
		MoveReparent(InEvents, Point);
		ReparentButton(InEvents, true);
	}
	else if (Phase == 2)
	{
		ReparentButton(InEvents, false);
	}
	else if (Phase == 3)
	{
		RequireReparent(Selection == Handle && Selection.All().size() == 1,
		                "viewport click must select the same object used by Outliner");
	}
	else if (Phase == 5 || Phase == 6 || Phase == 7)
	{
		RequireReparent(!ReparentGesture && !Gui->DragPayload(), "viewport must not start hierarchy drag");
		if (Phase == 5)
		{
			MoveReparent(InEvents, {Point.X + 20, Point.Y});
		}
		else if (Phase == 6)
		{
			MoveReparent(InEvents, RowCenter(Acceptance.MultiSelectionRows.at(Acceptance.ReparentExerciseIds[3])));
		}
		else
		{
			RequireReparent(Scene->FindNode(Handle)->Parent().empty(), "viewport drag changed parent");
			FInputEvent Event;
			Event.Type = EEventType::Key;
			Event.Key = EKey::Escape;
			Event.bDown = true;
			InEvents.push_back(Event);
		}
	}
	else if (Phase == 8)
	{
		ReparentButton(InEvents, false);
		FInputEvent Event;
		Event.Type = EEventType::Key;
		Event.Key = EKey::Escape;
		InEvents.push_back(Event);
	}
	else if (Phase == 9)
	{
		RequireReparent(Selection == Handle && HistoryCursor == Acceptance.ReparentExerciseHistory,
		                "selection or history changed after cancelled viewport gesture");
		MoveReparent(InEvents, RowCenter(Acceptance.MultiSelectionRows.at(Scene->FindNode(Handle)->Id)));
	}
	else if (Phase == 10)
	{
		// Let mouse/key release events drain before starting the next independent gesture.
		ReparentButton(InEvents, true);
	}
	else if (Phase == 11)
	{
		RequireReparent(ReparentGesture.has_value(), "Outliner did not use viewport selection");
		MoveReparent(InEvents, {ReparentGesture->Start.X + 20, ReparentGesture->Start.Y});
	}
	else if (Phase == 12)
	{
		MoveReparent(InEvents, RowCenter(Acceptance.MultiSelectionRows.at(Acceptance.ReparentExerciseIds[3])));
	}
	else if (Phase == 13)
	{
		RequireReparent(ReparentGesture && ReparentGesture->bTargetPreview, "Outliner target did not preview");
		ReparentButton(InEvents, false);
	}
	else
	{
		RequireReparent(Scene->FindNode(Handle)->Parent() == Acceptance.ReparentExerciseIds[3] &&
		                    HistoryCursor == Acceptance.ReparentExerciseHistory + 1 && Selection == Handle,
		                "Outliner reparent after viewport selection failed");
		Undo();
		RequireReparent(Scene->FindNode(Handle)->Parent().empty(), "selection exercise undo failed");
	}
	++Acceptance.ReparentSelectionStep;
}

void FEditorPlugin::ExerciseReparentInterruption(std::vector<FInputEvent>& InEvents)
{
	const auto Case = Acceptance.ReparentExerciseCase;
	RequireReparent(Case == 7 || (ReparentGesture && ReparentGesture->bTargetPreview),
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
		const auto B = Acceptance.ReparentExerciseNodes[1];
		auto Node = *Scene->FindNode(B);
		Node.Name += " changed";
		Scene->EditNode(B, Node, Scene->GetRevision());
		Acceptance.ReparentExerciseRevision = Scene->GetRevision();
	}
	if (Case == 12)
	{
		SceneDocument.Invalidate();
	}
	if (Case == 13)
	{
		ClickObject(Acceptance.ReparentExerciseNodes[2], true);
	}
}

void FEditorPlugin::ExerciseReparentDrag(std::vector<FInputEvent>& InEvents)
{
	const auto A = Acceptance.ReparentExerciseNodes[0];
	const auto B = Acceptance.ReparentExerciseNodes[1];
	const auto Child = Acceptance.ReparentExerciseNodes[2];
	const unsigned Case = Acceptance.ReparentExerciseCase;
	if (Acceptance.ReparentExerciseStep == 1)
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
			Selected = Acceptance.ReparentExerciseNodes[5];
			Selected.Toggle(B);
		}
		SetSelection(std::move(Selected));
		Filter = Case == 1 || Case == 9 ? "Reparent" : "";
		Acceptance.ReparentExerciseRevision = Scene->GetRevision();
		Acceptance.ReparentExerciseHistory = HistoryCursor;
		Error.clear();
	}
	else if (Acceptance.ReparentExerciseStep == 2)
	{
		const auto Point =
		    RowCenter(Acceptance.MultiSelectionRows.at(Acceptance.ReparentExerciseIds[Case == 9    ? 1
		                                                                              : Case == 10 ? 5
		                                                                                           : 0]));
		MoveReparent(InEvents, Point);
		ReparentButton(InEvents, true);
	}
	else if (Acceptance.ReparentExerciseStep == 3)
	{
		RequireReparent(ReparentGesture.has_value(), "press did not prepare source case " + std::to_string(Case));
		MoveReparent(InEvents, {ReparentGesture->Start.X + 20, ReparentGesture->Start.Y});
	}
	else if (Acceptance.ReparentExerciseStep == 4)
	{
		RequireReparent(ReparentGesture && ReparentGesture->bDragging, "drag did not start");
		RequireReparent(Selection.All().size() == (Case == 9    ? 1u
		                                           : Case == 10 ? 2u
		                                                        : 3u),
		                "drag collapsed the selection");
		const auto Bounds = Case == 1 || Case == 2
		                        ? InspectionBounds.at("hierarchy/root")
		                        : Acceptance.MultiSelectionRows.at(Acceptance.ReparentExerciseIds[Case == 3   ? 2
		                                                                                          : Case == 4 ? 4
		                                                                                                      : 3]);
		MoveReparent(InEvents, Case == 7 ? FVec2{5, 5} : RowCenter(Bounds));
	}
	else if (Acceptance.ReparentExerciseStep == 5)
	{
		ExerciseReparentInterruption(InEvents);
	}
	else if (Acceptance.ReparentExerciseStep == 6)
	{
		ReparentButton(InEvents, false);
	}
	else if (Acceptance.ReparentExerciseStep == 7)
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
		++Acceptance.ReparentExerciseCase;
		Acceptance.ReparentExerciseStep = 1;
		return;
	}
	++Acceptance.ReparentExerciseStep;
}

void FEditorPlugin::VerifyReparentExercise()
{
	const auto Case = Acceptance.ReparentExerciseCase;
	const bool bChanged = Case == 0 || Case == 1 || Case == 9 || Case == 10;
	RequireReparent(!ReparentGesture && !Gui->DragPayload(), "gesture survived delivery/cancellation");
	RequireReparent(HistoryCursor == Acceptance.ReparentExerciseHistory + (bChanged ? 1 : 0),
	                "unexpected history case " + std::to_string(Case) + " actual " + std::to_string(HistoryCursor) +
	                    ": " + Error);
	RequireReparent(Scene->GetRevision() == Acceptance.ReparentExerciseRevision + (bChanged ? 1 : 0),
	                "unexpected revision");
	RequireReparent(Scene->FindNode(Acceptance.ReparentExerciseNodes[2])->Parent() == Acceptance.ReparentExerciseIds[0],
	                "child was flattened");
	const auto Parent = Case == 0 || Case >= 9 ? Acceptance.ReparentExerciseIds[3] : std::string{};
	RequireReparent(Scene->FindNode(Acceptance.ReparentExerciseNodes[1])->Parent() == Parent, "incorrect parent");
	for (std::size_t Index = 0; Index < Acceptance.ReparentExerciseNodes.size(); ++Index)
	{
		FSceneNodeView View;
		Scene->GetNodeView(Acceptance.ReparentExerciseNodes[Index], View);
		for (std::size_t Element = 0; Element < View.World.Values.size(); ++Element)
		{
			RequireReparent(std::abs(View.World.Values[Element] -
			                         Acceptance.ReparentExerciseWorlds[Index].Values[Element]) < .0001f,
			                "world transform changed");
		}
	}
	if (bChanged)
	{
		const auto Selected = Selection;
		Undo();
		RequireReparent(HistoryCursor == Acceptance.ReparentExerciseHistory && Selection == Selected,
		                "undo selection/history");
		Redo();
		RequireReparent(Selection == Selected &&
		                    Scene->FindNode(Acceptance.ReparentExerciseNodes[1])->Parent() == Parent,
		                "redo parent/selection");
	}
}

void FEditorPlugin::ExerciseReparent(std::vector<FInputEvent>& InEvents)
{
	if (FrameCount < 12 || !Scene->GetStatus().bReady || !Viewport.bViewportVisible ||
	    GetPlacementPreparation(*PlacementRegistry.Find("Cube")).State != EPlacementPreparationState::Ready)
	{
		return;
	}
	if (Acceptance.ReparentExerciseStep == 0)
	{
		PrepareReparentExercise();
		return;
	}
	if (Acceptance.ReparentExerciseCase < 14)
	{
		if (Acceptance.ReparentKeyboardStep < 8)
		{
			ExerciseReparentKeyboard(InEvents);
			return;
		}
		if (Acceptance.ReparentSelectionStep < 30)
		{
			ExerciseReparentSelection(InEvents);
			return;
		}
		ExerciseReparentDrag(InEvents);
		return;
	}
	if (Acceptance.ReparentExerciseStep == 1)
	{
		SaveScene(Options.ExerciseReparent.generic_string());
		++Acceptance.ReparentExerciseStep;
	}
	else if (Acceptance.ReparentExerciseStep == 2 && !PendingSave)
	{
		RequireReparent(!IsDirty(), "save failed: " + Error);
		OpenScene(Options.ExerciseReparent.generic_string());
		++Acceptance.ReparentExerciseStep;
	}
	else if (Acceptance.ReparentExerciseStep == 3)
	{
		for (std::size_t Index = 0; Index < Acceptance.ReparentExerciseIds.size(); ++Index)
		{
			const auto Handle = Scene->FindHandle(Acceptance.ReparentExerciseIds[Index]);
			FSceneNodeView View;
			RequireReparent(Scene->GetNodeView(Handle, View), "saved node missing");
			const auto ExpectedParent = Index == 1 || Index == 5 ? Acceptance.ReparentExerciseIds[3]
			                            : Index == 2             ? Acceptance.ReparentExerciseIds[0]
			                                                     : "";
			RequireReparent(View.Node->Parent() == ExpectedParent, "saved parent changed");
			for (std::size_t Element = 0; Element < View.World.Values.size(); ++Element)
			{
				RequireReparent(std::abs(View.World.Values[Element] -
				                         Acceptance.ReparentExerciseWorlds[Index].Values[Element]) < .0001f,
				                "saved world changed");
			}
		}
		Acceptance.bReparentVerified = true;
	}
}
} // namespace Hyperion
