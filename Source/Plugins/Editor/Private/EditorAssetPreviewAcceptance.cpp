#include "EditorApplication.h"
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
	Event.Modifiers = 1;
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

bool FEditorPlugin::ExerciseAssetPreviewInput(std::vector<FInputEvent>& InEvents)
{
	auto& Exercise = AssetPreviewExercise;
	const auto* Document = AssetWorkspace->ActiveDocument();
	Exercise.bSawReady |= AssetWorkspace->IsPreviewReady();
	Exercise.bSawPreparing |= AssetWorkspace->ActiveStatus().find("0/1 models ready") != std::string::npos;
	if (Exercise.Step >= 4)
	{
		return ExerciseAssetPreviewHistory(InEvents);
	}
	if (Exercise.Step == 0)
	{
		AssetWorkspace->RevealProperty("node/position");
		Exercise.Before = HashArchive(Document->Get("nodes"));
		++Exercise.Step;
	}
	else if (Exercise.Step == 1)
	{
		if (++Exercise.Frames < 4)
		{
			return false;
		}
		const auto Field = AssetWorkspace->ObservedBounds("node/position/x");
		RequirePreview(Field.Z > Field.X && Field.W > Field.Y, "Model Position control is missing");
		Exercise.Canvas = AssetWorkspace->ObservedBounds("canvas");
		RequirePreview(Exercise.Canvas.W > Exercise.Canvas.Y, "Model preview canvas is missing");
		Exercise.Pointer = {(Field.X + Field.Z) * .5f, (Field.Y + Field.W) * .5f};
		PreviewPointer(InEvents, Exercise.Pointer);
		Exercise.Frames = 0;
		++Exercise.Step;
	}
	else if (Exercise.Step == 2)
	{
		PreviewPointer(InEvents, Exercise.Pointer, true);
		++Exercise.Step;
	}
	else
	{
		CheckCanvas(Exercise.Canvas, AssetWorkspace->ObservedBounds("canvas"));
		if (++Exercise.Frames <= 48)
		{
			Exercise.Pointer.X += 2;
			PreviewPointer(InEvents, Exercise.Pointer);
		}
		else
		{
			PreviewPointer(InEvents, Exercise.Pointer, false);
			Exercise.Frames = 0;
			++Exercise.Step;
		}
	}
	return false;
}

bool FEditorPlugin::ExerciseAssetPreviewHistory(std::vector<FInputEvent>& InEvents)
{
	auto& Exercise = AssetPreviewExercise;
	const auto* Document = AssetWorkspace->ActiveDocument();
	CheckCanvas(Exercise.Canvas, AssetWorkspace->ObservedBounds("canvas"));
	if (++Exercise.Frames < 3 || !AssetWorkspace->IsPreviewReady())
	{
		return false;
	}
	Exercise.Frames = 0;
	const auto Current = HashArchive(Document->Get("nodes"));
	if (Exercise.Step == 4)
	{
		RequirePreview(Current != Exercise.Before && Document->IsDirty(), "Position drag did not edit the model");
		RequirePreview(Exercise.bSawReady, "Preview layout regression did not observe a ready preview");
		RequirePreview(Exercise.bSawPreparing, "Preview layout regression did not observe a preparing preview");
		Exercise.After = Current;
		PreviewShortcut(InEvents, EKey::Z);
	}
	else if (Exercise.Step == 5)
	{
		RequirePreview(Current == Exercise.Before && !Document->IsDirty(), "Position drag Undo was not atomic");
		PreviewShortcut(InEvents, EKey::Y);
	}
	else if (Exercise.Step == 6)
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
	++Exercise.Step;
	return false;
}
} // namespace Hyperion
