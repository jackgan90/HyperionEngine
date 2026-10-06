#include "EditorAcceptanceHarness.h"
#include <cmath>

namespace Hyperion
{
namespace
{
void RequireGizmo(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(std::string("Gizmo acceptance: ") + InMessage);
	}
}

void PointerEvent(std::vector<FInputEvent>& InEvents, FVec2 InPosition, bool bInButton, bool bInDown)
{
	FInputEvent Event;
	Event.Type = bInButton ? EEventType::MouseButton : EEventType::MouseMove;
	Event.X = InPosition.X;
	Event.Y = InPosition.Y;
	Event.Button = InputButtons::Left;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}
} // namespace

void FEditorAcceptanceHarness::ExerciseGizmoInput(std::vector<FInputEvent>& InEvents)
{
	if (Editor.ReadyFrames < 10 || !Editor.Selection)
	{
		return;
	}
	if (Scenario.Gizmo.Is(EGizmoState::Complete))
	{
		Scenario.bGizmoVerified = true;
		return;
	}
	const auto Context = DescribeGizmoContext(Scenario.Gizmo.GetState());
	const unsigned Mode = Context.CaseIndex;
	const auto Phase = Context.Action;
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	const float Size = 85 * Editor.Gui->ApplicationScale();
	const float Diagonal = Size / std::sqrt(2.f);
	const FVec2 Start = Mode == 1 ? FVec2{Center.X + Diagonal, Center.Y - Diagonal} : Center;
	const FVec2 End = Mode == 1 ? FVec2{Center.X - Diagonal, Center.Y - Diagonal} : FVec2{Center.X + Size, Center.Y};
	if (Phase == EGizmoAction::PrepareModel)
	{
		// Place the selected model origin at the center with a deterministic camera and local transform.
		auto Candidate = *Editor.Scene->FindNode(*Editor.Selection);
		Candidate.Parent().clear();
		Candidate.Local() = Mode == 2 ? Scale({0, -1, 1}) : Identity();
		Editor.Scene->EditNode(*Editor.Selection, Candidate, Editor.Scene->GetRevision());
		Editor.ResetDocument();
		Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
		Scenario.GizmoExerciseCamera = Editor.Viewport.ViewCamera;
		Scenario.GizmoExerciseBefore = Candidate.Local();
	}
	if (Phase == EGizmoAction::PressMode || Phase == EGizmoAction::ReleaseMode)
	{
		const auto ButtonBounds = Scenario.GizmoButtonBounds[Mode];
		const FVec2 Point{(ButtonBounds.X + ButtonBounds.Z) / 2, (ButtonBounds.Y + ButtonBounds.W) / 2};
		PointerEvent(InEvents, Point, false, false);
		PointerEvent(InEvents, Point, true, Phase == EGizmoAction::PressMode);
	}
	if (Phase == EGizmoAction::BeginDrag)
	{
		RequireGizmo(static_cast<unsigned>(Editor.GizmoMode) == Mode, "toolbar mode selection");
		PointerEvent(InEvents, Start, false, false);
		PointerEvent(InEvents, Start, true, true);
	}
	if (Phase == EGizmoAction::MoveDrag)
	{
		RequireGizmo(Editor.Gizmo.IsDragging(), "pointer did not capture the handle");
		PointerEvent(InEvents, End, false, false);
	}
	if (Phase == EGizmoAction::VerifyPreviewAndRelease)
	{
		if (Editor.Scene->FindNode(*Editor.Selection)->Local().Values == Scenario.GizmoExerciseBefore.Values)
		{
			throw std::runtime_error("Gizmo live preview unchanged: mode=" + std::to_string(Mode) +
			                         " dragging=" + std::to_string(Editor.Gizmo.IsDragging()) +
			                         " error=" + Editor.Error + " history=" + std::to_string(Editor.HistoryCursor));
		}
		RequireGizmo(Editor.HistoryCursor == 0 && Editor.IsDirty(), "preview must not create history entries");
		if (Mode == 0)
		{
			// Resource/other property updates may advance the scene revision while a drag is active.
			auto Updated = *Editor.Scene->FindNode(*Editor.Selection);
			Updated.Name += " revised";
			Editor.Scene->EditNode(*Editor.Selection, std::move(Updated), Editor.Scene->GetRevision());
		}
		PointerEvent(InEvents, End, true, false);
	}
	if (Phase == EGizmoAction::BeginCancelledDrag)
	{
		PointerEvent(InEvents, Start, false, false);
		PointerEvent(InEvents, Start, true, true);
	}
	if (Phase == EGizmoAction::MoveCancelledDrag)
	{
		PointerEvent(InEvents, End, false, false);
	}
	if (Phase == EGizmoAction::PressEscape || Phase == EGizmoAction::ReleaseEscape)
	{
		FInputEvent Escape;
		Escape.Type = EEventType::Key;
		Escape.Key = EKey::Escape;
		Escape.bDown = Phase == EGizmoAction::PressEscape;
		InEvents.push_back(Escape);
		PointerEvent(InEvents, End, true, false);
	}
	CheckGizmoHistory(Phase);
	ExerciseGizmoFocus(Phase, Start, End, InEvents);
	switch (Scenario.Gizmo.GetState())
	{
		case EGizmoState::PositionPrepareModel:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionPressMode);
			break;
		case EGizmoState::PositionPressMode:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionReleaseMode);
			break;
		case EGizmoState::PositionReleaseMode:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionBeginDrag);
			break;
		case EGizmoState::PositionBeginDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionMoveDrag);
			break;
		case EGizmoState::PositionMoveDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionVerifyPreviewAndRelease);
			break;
		case EGizmoState::PositionVerifyPreviewAndRelease:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionVerifyHistory);
			break;
		case EGizmoState::PositionVerifyHistory:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionBeginCancelledDrag);
			break;
		case EGizmoState::PositionBeginCancelledDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionMoveCancelledDrag);
			break;
		case EGizmoState::PositionMoveCancelledDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionPressEscape);
			break;
		case EGizmoState::PositionPressEscape:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionReleaseEscape);
			break;
		case EGizmoState::PositionReleaseEscape:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionVerifyCancellation);
			break;
		case EGizmoState::PositionVerifyCancellation:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionBeginFocusDrag);
			break;
		case EGizmoState::PositionBeginFocusDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionMoveFocusDrag);
			break;
		case EGizmoState::PositionMoveFocusDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionLoseFocus);
			break;
		case EGizmoState::PositionLoseFocus:
			Scenario.Gizmo.TransitionTo(EGizmoState::PositionVerifyFocusCommit);
			break;
		case EGizmoState::PositionVerifyFocusCommit:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationPrepareModel);
			break;
		case EGizmoState::RotationPrepareModel:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationPressMode);
			break;
		case EGizmoState::RotationPressMode:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationReleaseMode);
			break;
		case EGizmoState::RotationReleaseMode:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationBeginDrag);
			break;
		case EGizmoState::RotationBeginDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationMoveDrag);
			break;
		case EGizmoState::RotationMoveDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationVerifyPreviewAndRelease);
			break;
		case EGizmoState::RotationVerifyPreviewAndRelease:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationVerifyHistory);
			break;
		case EGizmoState::RotationVerifyHistory:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationBeginCancelledDrag);
			break;
		case EGizmoState::RotationBeginCancelledDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationMoveCancelledDrag);
			break;
		case EGizmoState::RotationMoveCancelledDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationPressEscape);
			break;
		case EGizmoState::RotationPressEscape:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationReleaseEscape);
			break;
		case EGizmoState::RotationReleaseEscape:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationVerifyCancellation);
			break;
		case EGizmoState::RotationVerifyCancellation:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationBeginFocusDrag);
			break;
		case EGizmoState::RotationBeginFocusDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationMoveFocusDrag);
			break;
		case EGizmoState::RotationMoveFocusDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationLoseFocus);
			break;
		case EGizmoState::RotationLoseFocus:
			Scenario.Gizmo.TransitionTo(EGizmoState::RotationVerifyFocusCommit);
			break;
		case EGizmoState::RotationVerifyFocusCommit:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScalePrepareModel);
			break;
		case EGizmoState::ScalePrepareModel:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScalePressMode);
			break;
		case EGizmoState::ScalePressMode:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleReleaseMode);
			break;
		case EGizmoState::ScaleReleaseMode:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleBeginDrag);
			break;
		case EGizmoState::ScaleBeginDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleMoveDrag);
			break;
		case EGizmoState::ScaleMoveDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleVerifyPreviewAndRelease);
			break;
		case EGizmoState::ScaleVerifyPreviewAndRelease:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleVerifyHistory);
			break;
		case EGizmoState::ScaleVerifyHistory:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleBeginCancelledDrag);
			break;
		case EGizmoState::ScaleBeginCancelledDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleMoveCancelledDrag);
			break;
		case EGizmoState::ScaleMoveCancelledDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScalePressEscape);
			break;
		case EGizmoState::ScalePressEscape:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleReleaseEscape);
			break;
		case EGizmoState::ScaleReleaseEscape:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleVerifyCancellation);
			break;
		case EGizmoState::ScaleVerifyCancellation:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleBeginFocusDrag);
			break;
		case EGizmoState::ScaleBeginFocusDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleMoveFocusDrag);
			break;
		case EGizmoState::ScaleMoveFocusDrag:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleLoseFocus);
			break;
		case EGizmoState::ScaleLoseFocus:
			Scenario.Gizmo.TransitionTo(EGizmoState::ScaleVerifyFocusCommit);
			break;
		case EGizmoState::ScaleVerifyFocusCommit:
			Scenario.Gizmo.TransitionTo(EGizmoState::Complete);
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseGizmoFocus(EGizmoAction InPhase, FVec2 InStart, FVec2 InEnd,
                                                  std::vector<FInputEvent>& InEvents)
{
	if (InPhase == EGizmoAction::BeginFocusDrag)
	{
		Editor.Undo();
		PointerEvent(InEvents, InStart, false, false);
		PointerEvent(InEvents, InStart, true, true);
	}
	if (InPhase == EGizmoAction::MoveFocusDrag)
	{
		RequireGizmo(Editor.Gizmo.IsDragging(), "focus-loss drag did not start");
		PointerEvent(InEvents, InEnd, false, false);
	}
	if (InPhase == EGizmoAction::LoseFocus)
	{
		Scenario.GizmoExerciseAfter = Editor.Scene->FindNode(*Editor.Selection)->Local();
		RequireGizmo(Scenario.GizmoExerciseAfter.Values != Scenario.GizmoExerciseBefore.Values,
		             "focus-loss preview unchanged");
		RequireGizmo(Editor.HistoryCursor == 0, "focus-loss preview created history");
	}
	if (InPhase == EGizmoAction::VerifyFocusCommit)
	{
		RequireGizmo(!Editor.Gizmo.IsDragging() && Editor.HistoryCursor == 1, "focus loss must commit one command");
		RequireGizmo(Editor.Scene->FindNode(*Editor.Selection)->Local().Values == Scenario.GizmoExerciseAfter.Values,
		             "focus loss changed the last valid preview");
		Editor.Undo();
		RequireGizmo(Editor.Scene->FindNode(*Editor.Selection)->Local().Values == Scenario.GizmoExerciseBefore.Values,
		             "focus-loss undo");
		Editor.Redo();
		RequireGizmo(Editor.Scene->FindNode(*Editor.Selection)->Local().Values == Scenario.GizmoExerciseAfter.Values,
		             "focus-loss redo");
	}
	if (InPhase == EGizmoAction::LoseFocus || InPhase == EGizmoAction::VerifyFocusCommit)
	{
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = InPhase == EGizmoAction::VerifyFocusCommit;
		InEvents.push_back(Focus);
	}
}

void FEditorAcceptanceHarness::CheckGizmoHistory(EGizmoAction InPhase)
{
	if (InPhase == EGizmoAction::VerifyHistory)
	{
		RequireGizmo(!Editor.Gizmo.IsDragging() && Editor.HistoryCursor == 1, "release must commit one command");
		RequireGizmo(Editor.Viewport.ViewCamera == Scenario.GizmoExerciseCamera, "gizmo moved the viewport camera");
		Scenario.GizmoExerciseAfter = Editor.Scene->FindNode(*Editor.Selection)->Local();
		Editor.Undo();
		RequireGizmo(Editor.Scene->FindNode(*Editor.Selection)->Local().Values == Scenario.GizmoExerciseBefore.Values,
		             "undo");
		RequireGizmo(Editor.Scene->FindNode(*Editor.Selection)->Name.ends_with(" revised"),
		             "unrelated property was lost");
		Editor.Redo();
		RequireGizmo(Editor.Scene->FindNode(*Editor.Selection)->Local().Values == Scenario.GizmoExerciseAfter.Values,
		             "redo");
		Editor.Undo();
	}
	if (InPhase == EGizmoAction::VerifyCancellation)
	{
		RequireGizmo(!Editor.Gizmo.IsDragging() && Editor.HistoryCursor == 0, "escape cancellation history");
		RequireGizmo(Editor.Scene->FindNode(*Editor.Selection)->Local().Values == Scenario.GizmoExerciseBefore.Values,
		             "escape baseline");
		Editor.Redo();
		RequireGizmo(Editor.Scene->FindNode(*Editor.Selection)->Local().Values == Scenario.GizmoExerciseAfter.Values,
		             "cancel preserved redo");
	}
}
} // namespace Hyperion
