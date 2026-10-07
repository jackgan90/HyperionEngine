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

constexpr float DragDistance = 20.f;
constexpr float WorldTransformTolerance = .0001f;
constexpr unsigned MinimumReadyFrames = 12;
constexpr FVec2 OutsideDropPoint{5, 5};

enum class EFixtureKind
{
	Model,
	Folder,
	PointLight,
};

struct FFixtureDefinition
{
	EReparentFixtureRole Role;
	std::string_view Name;
	std::string_view Id;
	EFixtureKind Kind;
	std::string_view Parent;
	FMat4 Local;
};

const FFixtureDefinition FixtureDefinitions[]{
    {EReparentFixtureRole::PrimaryModel, "Reparent 0", "reparent-0", EFixtureKind::Model, "", Translation({-1, 0, 0})},
    {EReparentFixtureRole::SecondaryModel, "Reparent 1", "reparent-1", EFixtureKind::Model, "", Translation({1, 1, 0})},
    {EReparentFixtureRole::PrimaryChild, "Reparent 2", "reparent-2", EFixtureKind::Folder, "reparent-0",
     Translation({0, 2, 0})},
    {EReparentFixtureRole::ParentTarget, "Reparent 3", "reparent-3", EFixtureKind::Folder, "",
     ComposeTRS({3, 2, -1}, {0, std::sin(.2f), 0, std::cos(.2f)}, {-2, .5f, 3})},
    {EReparentFixtureRole::SingularTarget, "Reparent 4", "reparent-4", EFixtureKind::Folder, "", Scale({1, 0, 1})},
    {EReparentFixtureRole::PointLight, "Reparent 5", "reparent-5", EFixtureKind::PointLight, "",
     Translation({-2, 2, 0})},
};

void CaptureExpectedParents(const FSceneInstance& InScene, FReparentAcceptanceContext& OutExercise,
                            const FReparentCaseDefinition& InCase)
{
	for (auto& Fixture : OutExercise.Fixtures)
	{
		Fixture.BeforeParent = InScene.FindNode(Fixture.Handle)->Parent();
		Fixture.ExpectedParent = Fixture.BeforeParent;
	}
	if (InCase.Outcome == EReparentOutcome::Changed)
	{
		const auto* Target = std::get_if<EReparentFixtureRole>(&InCase.Target);
		const auto Parent = Target ? OutExercise.Fixture(*Target).Id : std::string{};
		for (const auto Role : InCase.MovedRoots)
		{
			OutExercise.Fixture(Role).ExpectedParent = Parent;
		}
	}
}

void RequireFixtureState(const FSceneInstance& InScene, const FReparentAcceptanceContext& InExercise,
                         bool bInBefore = false)
{
	for (const auto& Fixture : InExercise.Fixtures)
	{
		FSceneNodeView View;
		const auto Handle = InScene.FindHandle(Fixture.Id);
		RequireReparent(InScene.GetNodeView(Handle, View), "fixture missing: " + Fixture.Id);
		if (Fixture.Role == EReparentFixtureRole::PrimaryChild)
		{
			RequireReparent(View.Node->Parent() == InExercise.Fixture(EReparentFixtureRole::PrimaryModel).Id,
			                "child was flattened");
		}
		RequireReparent(View.Node->Parent() == (bInBefore ? Fixture.BeforeParent : Fixture.ExpectedParent),
		                "incorrect parent: " + Fixture.Id);
		for (std::size_t Element = 0; Element < View.World.Values.size(); ++Element)
		{
			RequireReparent(std::abs(View.World.Values[Element] - Fixture.InitialWorld.Values[Element]) <
			                    WorldTransformTolerance,
			                "world transform changed: " + Fixture.Id);
		}
	}
}

bool SameBounds(FVec4 InA, FVec4 InB)
{
	return InA.X == InB.X && InA.Y == InB.Y && InA.Z == InB.Z && InA.W == InB.W;
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
	for (const auto& Definition : FixtureDefinitions)
	{
		FSceneNode Node;
		Node.Name = Definition.Name;
		Node.Id = Definition.Id;
		Node.Parent() = Definition.Parent;
		Node.Local() = Definition.Local;
		if (Definition.Kind == EFixtureKind::Model)
		{
			Node.Model() = FSceneModelComponent{};
			Node.Model()->Asset = Editor.PlacementModels.at("Cube").Asset;
		}
		else if (Definition.Kind == EFixtureKind::PointLight)
		{
			Node.PointLight() = FScenePointLight{};
		}
		const auto Handle = Editor.Scene->AddNode(Node);
		FSceneNodeView View;
		RequireReparent(Editor.Scene->GetNodeView(Handle, View), "fixture preparation failed");
		Scenario.ReparentExercise.Fixtures.push_back(
		    {Definition.Role, Handle, Node.Id, View.World, Node.Parent(), Node.Parent()});
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
		MoveReparent(InEvents, RowCenter(Scenario.MultiSelectionRows.at(
		                           Scenario.ReparentExercise.Fixture(EReparentFixtureRole::PrimaryModel).Id)));
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
		RequireReparent(Editor.Selection ==
		                        Scenario.ReparentExercise.Fixture(EReparentFixtureRole::SecondaryModel).Handle &&
		                    Editor.Selection.All().size() == 1,
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
	const bool bLight = DescribeReparentSelectionContext(Scenario.ReparentSelection.GetState()).Subject ==
	                    EReparentSelectionSubject::Light;
	const auto Phase = DescribeReparentSelectionContext(Scenario.ReparentSelection.GetState()).Action;
	const auto Handle = Scenario.ReparentExercise
	                        .Fixture(bLight ? EReparentFixtureRole::PointLight : EReparentFixtureRole::PrimaryModel)
	                        .Handle;
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
			MoveReparent(InEvents, {Point.X + DragDistance, Point.Y});
		}
		else if (Phase == EReparentSelectionAction::MoveToOutliner)
		{
			MoveReparent(InEvents, RowCenter(Scenario.MultiSelectionRows.at(
			                           Scenario.ReparentExercise.Fixture(EReparentFixtureRole::ParentTarget).Id)));
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
		MoveReparent(InEvents,
		             {Editor.Reparent.GetGesture()->Start.X + DragDistance, Editor.Reparent.GetGesture()->Start.Y});
	}
	else if (Phase == EReparentSelectionAction::MoveOutlinerTarget)
	{
		MoveReparent(InEvents, RowCenter(Scenario.MultiSelectionRows.at(
		                           Scenario.ReparentExercise.Fixture(EReparentFixtureRole::ParentTarget).Id)));
	}
	else if (Phase == EReparentSelectionAction::ReleaseOutlinerDrag)
	{
		RequireReparent(Editor.Reparent.GetGesture() && Editor.Reparent.GetGesture()->bTargetPreview,
		                "Outliner target did not preview");
		ReparentButton(InEvents, false);
	}
	else
	{
		RequireReparent(Editor.Scene->FindNode(Handle)->Parent() ==
		                        Scenario.ReparentExercise.Fixture(EReparentFixtureRole::ParentTarget).Id &&
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
	const auto& Case = ReparentCases[Scenario.ReparentExercise.CaseIndex];
	RequireReparent(std::holds_alternative<FOutsideTarget>(Case.Target) ||
	                    (Editor.Reparent.GetGesture() && Editor.Reparent.GetGesture()->bTargetPreview),
	                "target did not preview: " + std::string(Case.Name));
	switch (Case.Interruption)
	{
		case EReparentInterruption::Escape:
		case EReparentInterruption::FocusLoss:
		case EReparentInterruption::RightButton:
		{
			FInputEvent Event;
			Event.Type = Case.Interruption == EReparentInterruption::Escape      ? EEventType::Key
			             : Case.Interruption == EReparentInterruption::FocusLoss ? EEventType::Focus
			                                                                     : EEventType::MouseButton;
			Event.Key = EKey::Escape;
			Event.Button = InputButtons::Right;
			Event.bDown = Case.Interruption != EReparentInterruption::FocusLoss;
			InEvents.push_back(Event);
			break;
		}
		case EReparentInterruption::RevisionChange:
		{
			const auto Handle = Scenario.ReparentExercise.Fixture(EReparentFixtureRole::SecondaryModel).Handle;
			auto Node = *Editor.Scene->FindNode(Handle);
			Node.Name += " changed";
			Editor.Scene->EditNode(Handle, Node, Editor.Scene->GetRevision());
			Scenario.ReparentExerciseRevision = Editor.Scene->GetRevision();
			break;
		}
		case EReparentInterruption::DocumentInvalidation:
			Editor.SceneDocument.Invalidate();
			break;
		case EReparentInterruption::SelectionChange:
			Editor.ClickObject(Scenario.ReparentExercise.Fixture(EReparentFixtureRole::PrimaryChild).Handle, true);
			break;
		case EReparentInterruption::None:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseReparentPreview(std::vector<FInputEvent>& InEvents)
{
	const auto& Exercise = Scenario.ReparentExercise;
	const auto& Case = ReparentCases[Exercise.CaseIndex];
	RequireReparent(Editor.Reparent.GetGesture() && Editor.Reparent.GetGesture()->bDragging, "drag did not start");
	RequireReparent(SameBounds(Scenario.ReparentSceneBounds, Scenario.Bounds.Require(EEditorWidget::HierarchyRoot)),
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
	RequireReparent(Editor.Selection.All().size() == Case.Selection.size(), "drag collapsed the selection");
	FVec2 TargetPoint;
	if (std::holds_alternative<FOutsideTarget>(Case.Target))
	{
		TargetPoint = OutsideDropPoint;
	}
	else if (const auto* Target = std::get_if<EReparentFixtureRole>(&Case.Target))
	{
		TargetPoint = RowCenter(Scenario.MultiSelectionRows.at(Exercise.Fixture(*Target).Id));
	}
	else
	{
		TargetPoint = RowCenter(Scenario.Bounds.Require(EEditorWidget::HierarchyRoot));
	}
	MoveReparent(InEvents, TargetPoint);
}

void FEditorAcceptanceHarness::ExerciseReparentDrag(std::vector<FInputEvent>& InEvents)
{
	auto& Exercise = Scenario.ReparentExercise;
	const auto& Case = ReparentCases[Exercise.CaseIndex];
	if (Scenario.ReparentDrag.Is(EReparentDragState::PrepareSelection))
	{
		FEditorSelection Selected;
		for (const auto Role : Case.Selection)
		{
			Selected.Toggle(Exercise.Fixture(Role).Handle);
		}
		Editor.SetSelection(std::move(Selected));
		Editor.Filter = Case.bFiltered ? "Reparent" : "";
		// Validate the declared/previous outcome before accepting the current topology as this case's snapshot.
		RequireFixtureState(*Editor.Scene, Exercise);
		CaptureExpectedParents(*Editor.Scene, Exercise, Case);
		Scenario.ReparentExerciseRevision = Editor.Scene->GetRevision();
		Scenario.ReparentExerciseHistory = Editor.HistoryCursor;
		Editor.Error.clear();
	}
	else if (Scenario.ReparentDrag.Is(EReparentDragState::PressSource))
	{
		Scenario.ReparentSceneBounds = Scenario.Bounds.Require(EEditorWidget::HierarchyRoot);
		Scenario.ReparentRowBounds = Scenario.MultiSelectionRows;
		const auto Point = RowCenter(Scenario.MultiSelectionRows.at(Exercise.Fixture(Case.Source).Id));
		MoveReparent(InEvents, Point);
		ReparentButton(InEvents, true);
	}
	else if (Scenario.ReparentDrag.Is(EReparentDragState::StartDrag))
	{
		RequireReparent(Editor.Reparent.GetGesture().has_value(),
		                "press did not prepare source: " + std::string(Case.Name));
		MoveReparent(InEvents,
		             {Editor.Reparent.GetGesture()->Start.X + DragDistance, Editor.Reparent.GetGesture()->Start.Y});
	}
	else if (Scenario.ReparentDrag.Is(EReparentDragState::PreviewTarget))
	{
		ExerciseReparentPreview(InEvents);
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
		Event.Type = Case.Interruption == EReparentInterruption::FocusLoss ? EEventType::Focus : EEventType::Key;
		Event.Key = EKey::Escape;
		Event.bDown = Case.Interruption == EReparentInterruption::FocusLoss;
		if (Case.Interruption == EReparentInterruption::RightButton)
		{
			Event.Type = EEventType::MouseButton;
			Event.Button = InputButtons::Right;
		}
		InEvents.push_back(Event);
	}
	else
	{
		VerifyReparentExercise();
		++Exercise.CaseIndex;
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
	const auto& Exercise = Scenario.ReparentExercise;
	const auto& Case = ReparentCases[Exercise.CaseIndex];
	const bool bChanged = Case.Outcome == EReparentOutcome::Changed;
	const unsigned ExpectedEdits = bChanged ? 1 : 0;
	RequireReparent(!Editor.Reparent.GetGesture() && !Editor.Gui->DragPayload(),
	                "gesture survived delivery/cancellation");
	RequireReparent(Editor.HistoryCursor == Scenario.ReparentExerciseHistory + ExpectedEdits,
	                "unexpected history: " + std::string(Case.Name) + " actual " +
	                    std::to_string(Editor.HistoryCursor) + ": " + Editor.Error);
	RequireReparent(Editor.Scene->GetRevision() == Scenario.ReparentExerciseRevision + ExpectedEdits,
	                "unexpected revision: " + std::string(Case.Name));
	RequireFixtureState(*Editor.Scene, Exercise);
	if (bChanged)
	{
		const auto Selected = Editor.Selection;
		Editor.Undo();
		RequireReparent(Editor.HistoryCursor == Scenario.ReparentExerciseHistory && Editor.Selection == Selected,
		                "undo selection/history");
		RequireFixtureState(*Editor.Scene, Exercise, true);
		Editor.Redo();
		RequireReparent(Editor.Selection == Selected, "redo selection");
		RequireFixtureState(*Editor.Scene, Exercise);
	}
}

void FEditorAcceptanceHarness::ExerciseReparent(std::vector<FInputEvent>& InEvents)
{
	if (Editor.FrameCount < MinimumReadyFrames || !Editor.Scene->GetStatus().bReady ||
	    !Editor.Viewport.bViewportVisible ||
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
	if (Scenario.ReparentExercise.CaseIndex < std::size(ReparentCases))
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
		RequireFixtureState(*Editor.Scene, Scenario.ReparentExercise);
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
		const auto Handle =
		    Editor.Scene->FindHandle(Scenario.ReparentExercise.Fixture(EReparentFixtureRole::PrimaryModel).Id);
		const auto Name = Editor.Scene->FindNode(Handle)->Name + " replacement probe";
		SetSceneMetadata(Editor.SceneDocument,
		                 {Editor.SceneDocument.Id(), Editor.Scene->GetRevision(), {{Handle, Name, std::nullopt}}});
		RequireReparent(Editor.HistoryCursor > 0 && Editor.IsDirty(), "replacement fixture needs real history");
		Editor.SetSelection(FSceneSelection(Handle));
		MoveReparent(InEvents, RowCenter(Scenario.MultiSelectionRows.at(
		                           Scenario.ReparentExercise.Fixture(EReparentFixtureRole::PrimaryModel).Id)));
		ReparentButton(InEvents, true);
	}
	else if (Scenario.ReparentDocument.Is(EReparentDocumentState::StartReplacementDrag))
	{
		RequireReparent(Editor.Reparent.HasGesture(), "replacement fixture did not prepare a gesture");
		const auto Start = Editor.Reparent.GetGesture()->Start;
		MoveReparent(InEvents, {Start.X + DragDistance, Start.Y});
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
