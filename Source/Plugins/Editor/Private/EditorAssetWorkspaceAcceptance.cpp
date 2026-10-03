#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"
#include <array>
#include <cmath>

namespace Hyperion
{
namespace
{
constexpr std::array CloseNames{"CustomMaterial", "Texture", "Sky", "Radiance", "Material", "Model"};

void CheckWorkspace(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

void MovePointer(std::vector<FInputEvent>& InEvents, FVec2 InPoint)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
}

void PointerButton(std::vector<FInputEvent>& InEvents, FVec2 InPoint, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseButton;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}

void WorkspaceKey(std::vector<FInputEvent>& InEvents, EKey InKey, bool bInDown, unsigned InModifiers = 0)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.bDown = bInDown;
	Event.Modifiers = InModifiers;
	InEvents.push_back(Event);
}
} // namespace

void FEditorAcceptanceHarness::ExerciseAssetTabClose(std::vector<FInputEvent>& InEvents, const std::string& InPath)
{
	auto Bounds = Editor.AssetWorkspace->ObservedBounds("tab/" + InPath);
	CheckWorkspace(Bounds.Z > Bounds.X, "Asset tab missing before close");
	Bounds.X = Bounds.Z - Editor.Gui->Scale(28);
	MovePointer(InEvents, {(Bounds.X + Bounds.Z) * .5f, (Bounds.Y + Bounds.W) * .5f});
	if (Scenario.ExerciseWait < 30)
	{
		++Scenario.ExerciseWait;
		return;
	}
	ExerciseClick(InEvents, Bounds);
}

void FEditorAcceptanceHarness::ExerciseAssetWorkspaceInput(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.ExerciseStep >= 173)
	{
		ExerciseAssetPanelInput(InEvents);
		return;
	}
	if (Scenario.ExerciseStep >= 165)
	{
		ExerciseAssetDiscardInput(InEvents);
		return;
	}
	const auto Path = "/Game/" + std::string(CloseNames.at(Scenario.AssetExerciseIndex)) + ".hasset";
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 161:
			Editor.AssetWorkspace->Open(Path);
			++Scenario.ExerciseStep;
			break;
		case 162:
			if (Document && Document->Loaded().Path == Path && Editor.AssetWorkspace->IsPreviewReady())
			{
				CheckWorkspace(!Document->IsDirty(), "Clean close fixture is dirty");
				++Scenario.ExerciseStep;
			}
			break;
		case 163:
			ExerciseAssetTabClose(InEvents, Path);
			break;
		case 164:
			CheckWorkspace(Editor.AssetWorkspace->ObservedBounds("tab/" + Path).Z == 0 && !Editor.Window->ShouldClose(),
			               "Closing clean asset did not remove only its tab");
			CheckWorkspace(Editor.IsDirty() && Editor.History.size() == 1, "Closing asset changed scene history");
			Log(ELogLevel::Info, "Clean asset tab close passed: " + Path);
			Scenario.ExerciseStep = ++Scenario.AssetExerciseIndex < CloseNames.size() ? 161 : 165;
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetDiscardInput(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 165:
			Editor.AssetWorkspace->Open("/Game/Material.hasset");
			++Scenario.ExerciseStep;
			break;
		case 166:
			if (Document && Document->Loaded().Path == "/Game/Material.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				Scenario.AssetExerciseOriginalName = ReadValue<std::string>(Document->Get("name"));
				Editor.AssetWorkspace->RevealProperty("field/name");
				++Scenario.ExerciseStep;
			}
			break;
		case 167:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 168:
			if (++Scenario.ExerciseWait == 1)
			{
				WorkspaceKey(InEvents, EKey::A, true, 1);
			}
			if (Scenario.ExerciseWait == 2)
			{
				WorkspaceKey(InEvents, EKey::A, false);
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Discard this draft";
				InEvents.push_back(Text);
			}
			if (Scenario.ExerciseWait == 4)
			{
				WorkspaceKey(InEvents, EKey::Enter, true);
			}
			if (Scenario.ExerciseWait == 5)
			{
				WorkspaceKey(InEvents, EKey::Enter, false);
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		case 169:
			CheckWorkspace(Document && Document->IsDirty(), "Discard fixture was not edited");
			ExerciseAssetTabClose(InEvents, "/Game/Material.hasset");
			break;
		case 170:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("close/discard"));
			break;
		case 171:
			CheckWorkspace(Editor.AssetWorkspace->ObservedBounds("tab//Game/Material.hasset").Z == 0 &&
			                   !Editor.Window->ShouldClose(),
			               "Discard did not remove only its asset tab");
			Editor.AssetWorkspace->Open("/Game/Material.hasset");
			++Scenario.ExerciseStep;
			break;
		case 172:
			if (Document && Document->Loaded().Path == "/Game/Material.hasset" &&
			    Editor.AssetWorkspace->IsPreviewReady())
			{
				CheckWorkspace(!Document->IsDirty() &&
				                   ReadValue<std::string>(Document->Get("name")) == Scenario.AssetExerciseOriginalName,
				               "Discard persisted an unsaved edit");
				Log(ELogLevel::Info, "Dirty asset discard and reopen passed");
				Editor.AssetWorkspace->Open("/Game/Texture.hasset");
				Editor.AssetWorkspace->Open("/Game/Sky.hasset");
				++Scenario.ExerciseStep;
			}
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetPanelInput(std::vector<FInputEvent>& InEvents)
{
	switch (Scenario.ExerciseStep)
	{
		case 173:
			if (const auto* Document = Editor.AssetWorkspace->ActiveDocument();
			    Document && Document->Loaded().Path == "/Game/Sky.hasset" && Editor.AssetWorkspace->IsPreviewReady())
			{
				// Reproduce the user's floating panel, retaining the rest of the workspace layout.
				auto Layout = Editor.Gui->SaveLayout();
				const auto Start = Layout.find("[Window][Place Object]");
				const auto End = Layout.find("\n\n", Start);
				CheckWorkspace(Start != std::string::npos && End != std::string::npos,
				               "Place Object layout was not saved");
				Layout.replace(Start, End - Start, "[Window][Place Object]\nPos=400,200\nSize=300,500\nCollapsed=0");
				Editor.Gui->LoadLayout(Layout);
				Editor.bFocusPlacement = true;
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		case 174:
		{
			const auto Bounds = Scenario.InspectionBounds.at("placement/title");
			Scenario.AssetExercisePointer = {(Bounds.X + Bounds.Z) * .5f, (Bounds.Y + Bounds.W) * .5f};
			MovePointer(InEvents, Scenario.AssetExercisePointer);
			if (++Scenario.ExerciseWait == 3)
			{
				PointerButton(InEvents, Scenario.AssetExercisePointer, true);
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		}
		case 175:
			// Observe sustained movement across multiple held-button frames.
			++Scenario.ExerciseWait;
			if (Scenario.ExerciseWait == 6)
			{
				Scenario.AssetExercisePanelStart = Scenario.InspectionBounds.at("placement/title");
			}
			if (Scenario.ExerciseWait == 14)
			{
				const auto Bounds = Scenario.InspectionBounds.at("placement/title");
				CheckWorkspace(std::abs(Bounds.X - Scenario.AssetExercisePanelStart.X - 80) < 2 &&
				                   std::abs(Bounds.Y - Scenario.AssetExercisePanelStart.Y - 48) < 2,
				               "Place Object window drag was interrupted while an asset tab was active");
				PointerButton(InEvents, Scenario.AssetExercisePointer, false);
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
				break;
			}
			Scenario.AssetExercisePointer.X += 10;
			Scenario.AssetExercisePointer.Y += 6;
			MovePointer(InEvents, Scenario.AssetExercisePointer);
			break;
		case 176:
			// A docking request from release is applied at the next frame boundary.
			if (++Scenario.ExerciseWait < 3)
			{
				break;
			}
			CheckWorkspace(!Editor.Gui->PointerState().bDown, "Panel drag mouse release was not consumed");
			Scenario.AssetExercisePanelStart = Scenario.InspectionBounds.at("placement/title");
			MovePointer(InEvents, {Scenario.AssetExercisePointer.X + 50, Scenario.AssetExercisePointer.Y + 50});
			++Scenario.ExerciseStep;
			break;
		case 177:
		{
			const auto Bounds = Scenario.InspectionBounds.at("placement/title");
			CheckWorkspace(Bounds.X == Scenario.AssetExercisePanelStart.X &&
			                   Bounds.Y == Scenario.AssetExercisePanelStart.Y,
			               "Place Object window kept moving after mouse release");
			CheckWorkspace(!Editor.Placement.IsActive() && Editor.IsDirty() && Editor.History.size() == 1,
			               "Panel drag changed the scene");
			Log(ELogLevel::Info, "Place Object panel held-button drag and release passed with three asset tabs");
			Scenario.AssetExerciseIndex = 4;
			Scenario.ExerciseWait = 0;
			Editor.AssetWorkspace->RevealProperty("field/name");
			Scenario.ExerciseStep = 190;
			break;
		}
	}
}
} // namespace Hyperion
