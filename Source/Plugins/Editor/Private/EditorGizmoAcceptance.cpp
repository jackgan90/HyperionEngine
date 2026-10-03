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
	const unsigned Mode = Scenario.GizmoExerciseStep / 16;
	const unsigned Phase = Scenario.GizmoExerciseStep % 16;
	if (Mode >= 3)
	{
		Scenario.bGizmoVerified = true;
		return;
	}
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	const float Size = 85 * Editor.Gui->ApplicationScale();
	const float Diagonal = Size / std::sqrt(2.f);
	const FVec2 Start = Mode == 1 ? FVec2{Center.X + Diagonal, Center.Y - Diagonal} : Center;
	const FVec2 End = Mode == 1 ? FVec2{Center.X - Diagonal, Center.Y - Diagonal} : FVec2{Center.X + Size, Center.Y};
	if (Phase == 0)
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
	if (Phase == 1 || Phase == 2)
	{
		const auto ButtonBounds = Scenario.GizmoButtonBounds[Mode];
		const FVec2 Point{(ButtonBounds.X + ButtonBounds.Z) / 2, (ButtonBounds.Y + ButtonBounds.W) / 2};
		PointerEvent(InEvents, Point, false, false);
		PointerEvent(InEvents, Point, true, Phase == 1);
	}
	if (Phase == 3)
	{
		RequireGizmo(static_cast<unsigned>(Editor.GizmoMode) == Mode, "toolbar mode selection");
		PointerEvent(InEvents, Start, false, false);
		PointerEvent(InEvents, Start, true, true);
	}
	if (Phase == 4)
	{
		RequireGizmo(Editor.Gizmo.IsDragging(), "pointer did not capture the handle");
		PointerEvent(InEvents, End, false, false);
	}
	if (Phase == 5)
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
	if (Phase == 7)
	{
		PointerEvent(InEvents, Start, false, false);
		PointerEvent(InEvents, Start, true, true);
	}
	if (Phase == 8)
	{
		PointerEvent(InEvents, End, false, false);
	}
	if (Phase == 9 || Phase == 10)
	{
		FInputEvent Escape;
		Escape.Type = EEventType::Key;
		Escape.Key = EKey::Escape;
		Escape.bDown = Phase == 9;
		InEvents.push_back(Escape);
		PointerEvent(InEvents, End, true, false);
	}
	CheckGizmoHistory(Phase);
	ExerciseGizmoFocus(Phase, Start, End, InEvents);
	++Scenario.GizmoExerciseStep;
}

void FEditorAcceptanceHarness::ExerciseGizmoFocus(unsigned InPhase, FVec2 InStart, FVec2 InEnd,
                                                  std::vector<FInputEvent>& InEvents)
{
	if (InPhase == 12)
	{
		Editor.Undo();
		PointerEvent(InEvents, InStart, false, false);
		PointerEvent(InEvents, InStart, true, true);
	}
	if (InPhase == 13)
	{
		RequireGizmo(Editor.Gizmo.IsDragging(), "focus-loss drag did not start");
		PointerEvent(InEvents, InEnd, false, false);
	}
	if (InPhase == 14)
	{
		Scenario.GizmoExerciseAfter = Editor.Scene->FindNode(*Editor.Selection)->Local();
		RequireGizmo(Scenario.GizmoExerciseAfter.Values != Scenario.GizmoExerciseBefore.Values,
		             "focus-loss preview unchanged");
		RequireGizmo(Editor.HistoryCursor == 0, "focus-loss preview created history");
	}
	if (InPhase == 15)
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
	if (InPhase == 14 || InPhase == 15)
	{
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = InPhase == 15;
		InEvents.push_back(Focus);
	}
}

void FEditorAcceptanceHarness::CheckGizmoHistory(unsigned InPhase)
{
	if (InPhase == 6)
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
	if (InPhase == 11)
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
