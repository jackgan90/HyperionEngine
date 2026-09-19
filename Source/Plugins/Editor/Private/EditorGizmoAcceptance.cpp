#include "EditorApplication.h"
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
	Event.Button = 0;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}
} // namespace

void FEditorPlugin::ExerciseGizmoInput(std::vector<FInputEvent>& InEvents)
{
	if (ReadyFrames < 10 || !Selection)
	{
		return;
	}
	const unsigned Mode = GizmoExerciseStep / 16;
	const unsigned Phase = GizmoExerciseStep % 16;
	if (Mode >= 3)
	{
		bGizmoVerified = true;
		return;
	}
	const auto Bounds = ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	const float Size = 85 * Gui->ApplicationScale();
	const float Diagonal = Size / std::sqrt(2.f);
	const FVec2 Start = Mode == 1 ? FVec2{Center.X + Diagonal, Center.Y - Diagonal} : Center;
	const FVec2 End = Mode == 1 ? FVec2{Center.X - Diagonal, Center.Y - Diagonal} : FVec2{Center.X + Size, Center.Y};
	if (Phase == 0)
	{
		// Place the selected model origin at the center with a deterministic camera and local transform.
		auto Candidate = *Scene->FindNode(*Selection);
		Candidate.Parent().clear();
		Candidate.Local() = Mode == 2 ? Scale({0, -1, 1}) : Identity();
		Scene->EditNode(*Selection, Candidate, Scene->GetRevision());
		ResetDocument();
		ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
		GizmoExerciseCamera = ViewCamera;
		GizmoExerciseBefore = Candidate.Local();
	}
	if (Phase == 1 || Phase == 2)
	{
		const auto ButtonBounds = GizmoButtonBounds[Mode];
		const FVec2 Point{(ButtonBounds.X + ButtonBounds.Z) / 2, (ButtonBounds.Y + ButtonBounds.W) / 2};
		PointerEvent(InEvents, Point, false, false);
		PointerEvent(InEvents, Point, true, Phase == 1);
	}
	if (Phase == 3)
	{
		RequireGizmo(static_cast<unsigned>(GizmoMode) == Mode, "toolbar mode selection");
		PointerEvent(InEvents, Start, false, false);
		PointerEvent(InEvents, Start, true, true);
	}
	if (Phase == 4)
	{
		RequireGizmo(Gizmo.IsDragging(), "pointer did not capture the handle");
		PointerEvent(InEvents, End, false, false);
	}
	if (Phase == 5)
	{
		if (Scene->FindNode(*Selection)->Local().Values == GizmoExerciseBefore.Values)
		{
			throw std::runtime_error("Gizmo live preview unchanged: mode=" + std::to_string(Mode) +
			                         " dragging=" + std::to_string(Gizmo.IsDragging()) + " error=" + Error +
			                         " history=" + std::to_string(HistoryCursor));
		}
		RequireGizmo(HistoryCursor == 0 && IsDirty(), "preview must not create history entries");
		if (Mode == 0)
		{
			// Resource/other property updates may advance the scene revision while a drag is active.
			auto Updated = *Scene->FindNode(*Selection);
			Updated.Name += " revised";
			Scene->EditNode(*Selection, std::move(Updated), Scene->GetRevision());
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
	++GizmoExerciseStep;
}

void FEditorPlugin::ExerciseGizmoFocus(unsigned InPhase, FVec2 InStart, FVec2 InEnd, std::vector<FInputEvent>& InEvents)
{
	if (InPhase == 12)
	{
		Undo();
		PointerEvent(InEvents, InStart, false, false);
		PointerEvent(InEvents, InStart, true, true);
	}
	if (InPhase == 13)
	{
		RequireGizmo(Gizmo.IsDragging(), "focus-loss drag did not start");
		PointerEvent(InEvents, InEnd, false, false);
	}
	if (InPhase == 14)
	{
		GizmoExerciseAfter = Scene->FindNode(*Selection)->Local();
		RequireGizmo(GizmoExerciseAfter.Values != GizmoExerciseBefore.Values, "focus-loss preview unchanged");
		RequireGizmo(HistoryCursor == 0, "focus-loss preview created history");
	}
	if (InPhase == 15)
	{
		RequireGizmo(!Gizmo.IsDragging() && HistoryCursor == 1, "focus loss must commit one command");
		RequireGizmo(Scene->FindNode(*Selection)->Local().Values == GizmoExerciseAfter.Values,
		             "focus loss changed the last valid preview");
		Undo();
		RequireGizmo(Scene->FindNode(*Selection)->Local().Values == GizmoExerciseBefore.Values, "focus-loss undo");
		Redo();
		RequireGizmo(Scene->FindNode(*Selection)->Local().Values == GizmoExerciseAfter.Values, "focus-loss redo");
	}
	if (InPhase == 14 || InPhase == 15)
	{
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = InPhase == 15;
		InEvents.push_back(Focus);
	}
}

void FEditorPlugin::CheckGizmoHistory(unsigned InPhase)
{
	if (InPhase == 6)
	{
		RequireGizmo(!Gizmo.IsDragging() && HistoryCursor == 1, "release must commit one command");
		RequireGizmo(ViewCamera == GizmoExerciseCamera, "gizmo moved the viewport camera");
		GizmoExerciseAfter = Scene->FindNode(*Selection)->Local();
		Undo();
		RequireGizmo(Scene->FindNode(*Selection)->Local().Values == GizmoExerciseBefore.Values, "undo");
		RequireGizmo(Scene->FindNode(*Selection)->Name.ends_with(" revised"), "unrelated property was lost");
		Redo();
		RequireGizmo(Scene->FindNode(*Selection)->Local().Values == GizmoExerciseAfter.Values, "redo");
		Undo();
	}
	if (InPhase == 11)
	{
		RequireGizmo(!Gizmo.IsDragging() && HistoryCursor == 0, "escape cancellation history");
		RequireGizmo(Scene->FindNode(*Selection)->Local().Values == GizmoExerciseBefore.Values, "escape baseline");
		Redo();
		RequireGizmo(Scene->FindNode(*Selection)->Local().Values == GizmoExerciseAfter.Values, "cancel preserved redo");
	}
}
} // namespace Hyperion
