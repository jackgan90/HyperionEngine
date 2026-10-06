#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include <cmath>

namespace Hyperion
{
namespace
{
void RequirePreview(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

void PreviewPointer(std::vector<FInputEvent>& InEvents, FVec2 InPoint, std::optional<bool> InDown = {})
{
	FInputEvent Event;
	Event.Type = InDown ? EEventType::MouseButton : EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	Event.bDown = InDown.value_or(false);
	InEvents.push_back(Event);
}

void PreviewShortcut(std::vector<FInputEvent>& InEvents, EKey InKey)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.Modifiers = InputModifiers::Control;
	Event.bDown = true;
	InEvents.push_back(Event);
	Event.bDown = false;
	Event.Modifiers = 0;
	InEvents.push_back(Event);
}

void CheckCanvas(FVec4 InBefore, FVec4 InAfter)
{
	RequirePreview(std::abs(InBefore.X - InAfter.X) < .5f && std::abs(InBefore.Y - InAfter.Y) < .5f &&
	                   std::abs(InBefore.Z - InAfter.Z) < .5f && std::abs(InBefore.W - InAfter.W) < .5f,
	               "Model preview rectangle changed during Position drag or history refresh");
}
} // namespace

bool FEditorAcceptanceHarness::ExerciseAssetPreviewInput(std::vector<FInputEvent>& InEvents)
{
	auto& Exercise = Scenario.AssetPreviewExercise;
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	Exercise.bSawReady |= Editor.AssetWorkspace->IsPreviewReady();
	Exercise.bSawPreparing |= Editor.AssetWorkspace->ActiveStatus().find("0/1 models ready") != std::string::npos;
	if (Exercise.Progress.IsAny({EAssetPreviewState::VerifyDragAndUndo, EAssetPreviewState::VerifyUndoAndRedo,
	                             EAssetPreviewState::VerifyRedoAndRestore, EAssetPreviewState::VerifyRestoredPreview}))
	{
		return ExerciseAssetPreviewHistory(InEvents);
	}
	if (Exercise.Progress.Is(EAssetPreviewState::RevealPosition))
	{
		Editor.AssetWorkspace->RevealProperty("node/position");
		Exercise.Before = HashArchive(Document->Get("nodes"));
		Exercise.PositionControlSettle.Restart();
		Exercise.Progress.TransitionTo(EAssetPreviewState::AwaitPositionControl);
	}
	else if (Exercise.Progress.Is(EAssetPreviewState::AwaitPositionControl))
	{
		if (!Exercise.PositionControlSettle.Advance())
		{
			return false;
		}
		const auto Field = Editor.AssetWorkspace->ObservedBounds("node/position/x");
		RequirePreview(Field.Z > Field.X && Field.W > Field.Y, "Model Position control is missing");
		Exercise.Canvas = Editor.AssetWorkspace->ObservedBounds("canvas");
		RequirePreview(Exercise.Canvas.W > Exercise.Canvas.Y, "Model preview canvas is missing");
		Exercise.Pointer = {(Field.X + Field.Z) * .5f, (Field.Y + Field.W) * .5f};
		PreviewPointer(InEvents, Exercise.Pointer);
		Exercise.DragSample.Restart();
		Exercise.Progress.TransitionTo(EAssetPreviewState::PressPosition);
	}
	else if (Exercise.Progress.Is(EAssetPreviewState::PressPosition))
	{
		PreviewPointer(InEvents, Exercise.Pointer, true);
		Exercise.Progress.TransitionTo(EAssetPreviewState::DragPosition);
	}
	else
	{
		CheckCanvas(Exercise.Canvas, Editor.AssetWorkspace->ObservedBounds("canvas"));
		if (Exercise.DragSample.ConsumeFrame())
		{
			Exercise.Pointer.X += 2;
			PreviewPointer(InEvents, Exercise.Pointer);
		}
		else
		{
			PreviewPointer(InEvents, Exercise.Pointer, false);
			Exercise.HistorySettle.Restart();
			Exercise.Progress.TransitionTo(EAssetPreviewState::VerifyDragAndUndo);
		}
	}
	return false;
}

bool FEditorAcceptanceHarness::ExerciseAssetPreviewHistory(std::vector<FInputEvent>& InEvents)
{
	auto& Exercise = Scenario.AssetPreviewExercise;
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	CheckCanvas(Exercise.Canvas, Editor.AssetWorkspace->ObservedBounds("canvas"));
	if (!Exercise.HistorySettle.Advance() || !Editor.AssetWorkspace->IsPreviewReady())
	{
		return false;
	}
	Exercise.HistorySettle.Restart();
	const auto Current = HashArchive(Document->Get("nodes"));
	if (Exercise.Progress.Is(EAssetPreviewState::VerifyDragAndUndo))
	{
		RequirePreview(Current != Exercise.Before && Document->IsDirty(), "Position drag did not edit the model");
		RequirePreview(Exercise.bSawReady, "Preview layout regression did not observe a ready preview");
		RequirePreview(Exercise.bSawPreparing, "Preview layout regression did not observe a preparing preview");
		Exercise.After = Current;
		PreviewShortcut(InEvents, EKey::Z);
	}
	else if (Exercise.Progress.Is(EAssetPreviewState::VerifyUndoAndRedo))
	{
		RequirePreview(Current == Exercise.Before && !Document->IsDirty(), "Position drag Undo was not atomic");
		PreviewShortcut(InEvents, EKey::Y);
	}
	else if (Exercise.Progress.Is(EAssetPreviewState::VerifyRedoAndRestore))
	{
		RequirePreview(Current == Exercise.After && Document->IsDirty(), "Position drag Redo lost the edit");
		PreviewShortcut(InEvents, EKey::Z);
	}
	else
	{
		RequirePreview(Current == Exercise.Before && !Document->IsDirty(), "Model drag fixture was not restored");
		Log(ELogLevel::Info, "Model Position drag, readiness transitions and Undo/Redo retain the preview rectangle");
		return true;
	}
	switch (Exercise.Progress.GetState())
	{
		case EAssetPreviewState::VerifyDragAndUndo:
			Exercise.Progress.TransitionTo(EAssetPreviewState::VerifyUndoAndRedo);
			break;
		case EAssetPreviewState::VerifyUndoAndRedo:
			Exercise.Progress.TransitionTo(EAssetPreviewState::VerifyRedoAndRestore);
			break;
		case EAssetPreviewState::VerifyRedoAndRestore:
			Exercise.Progress.TransitionTo(EAssetPreviewState::VerifyRestoredPreview);
			break;
		default:
			break;
	}
	return false;
}
} // namespace Hyperion
