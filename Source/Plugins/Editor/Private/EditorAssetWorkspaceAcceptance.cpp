#include "EditorApplication.h"
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

void FEditorPlugin::ExerciseAssetTabClose(std::vector<FInputEvent>& InEvents, const std::string& InPath)
{
	auto Bounds = AssetWorkspace->ObservedBounds("tab/" + InPath);
	CheckWorkspace(Bounds.Z > Bounds.X, "Asset tab missing before close");
	Bounds.X = Bounds.Z - Gui->Scale(28);
	MovePointer(InEvents, {(Bounds.X + Bounds.Z) * .5f, (Bounds.Y + Bounds.W) * .5f});
	if (Acceptance.ExerciseWait < 30)
	{
		++Acceptance.ExerciseWait;
		return;
	}
	ExerciseClick(InEvents, Bounds);
}

void FEditorPlugin::ExerciseAssetWorkspaceInput(std::vector<FInputEvent>& InEvents)
{
	if (Acceptance.ExerciseStep >= 173)
	{
		ExerciseAssetPanelInput(InEvents);
		return;
	}
	if (Acceptance.ExerciseStep >= 165)
	{
		ExerciseAssetDiscardInput(InEvents);
		return;
	}
	const auto Path = "/Game/" + std::string(CloseNames.at(Acceptance.AssetExerciseIndex)) + ".hasset";
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (Acceptance.ExerciseStep)
	{
		case 161:
			AssetWorkspace->Open(Path);
			++Acceptance.ExerciseStep;
			break;
		case 162:
			if (Document && Document->Loaded().Path == Path && AssetWorkspace->IsPreviewReady())
			{
				CheckWorkspace(!Document->IsDirty(), "Clean close fixture is dirty");
				++Acceptance.ExerciseStep;
			}
			break;
		case 163:
			ExerciseAssetTabClose(InEvents, Path);
			break;
		case 164:
			CheckWorkspace(AssetWorkspace->ObservedBounds("tab/" + Path).Z == 0 && !Window->ShouldClose(),
			               "Closing clean asset did not remove only its tab");
			CheckWorkspace(IsDirty() && History.size() == 1, "Closing asset changed scene history");
			Log(ELogLevel::Info, "Clean asset tab close passed: " + Path);
			Acceptance.ExerciseStep = ++Acceptance.AssetExerciseIndex < CloseNames.size() ? 161 : 165;
			break;
	}
}

void FEditorPlugin::ExerciseAssetDiscardInput(std::vector<FInputEvent>& InEvents)
{
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (Acceptance.ExerciseStep)
	{
		case 165:
			AssetWorkspace->Open("/Game/Material.hasset");
			++Acceptance.ExerciseStep;
			break;
		case 166:
			if (Document && Document->Loaded().Path == "/Game/Material.hasset" && AssetWorkspace->IsPreviewReady())
			{
				Acceptance.AssetExerciseOriginalName = ReadValue<std::string>(Document->Get("name"));
				AssetWorkspace->RevealProperty("field/name");
				++Acceptance.ExerciseStep;
			}
			break;
		case 167:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 168:
			if (++Acceptance.ExerciseWait == 1)
			{
				WorkspaceKey(InEvents, EKey::A, true, 1);
			}
			if (Acceptance.ExerciseWait == 2)
			{
				WorkspaceKey(InEvents, EKey::A, false);
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = "Discard this draft";
				InEvents.push_back(Text);
			}
			if (Acceptance.ExerciseWait == 4)
			{
				WorkspaceKey(InEvents, EKey::Enter, true);
			}
			if (Acceptance.ExerciseWait == 5)
			{
				WorkspaceKey(InEvents, EKey::Enter, false);
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
			}
			break;
		case 169:
			CheckWorkspace(Document && Document->IsDirty(), "Discard fixture was not edited");
			ExerciseAssetTabClose(InEvents, "/Game/Material.hasset");
			break;
		case 170:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("close/discard"));
			break;
		case 171:
			CheckWorkspace(AssetWorkspace->ObservedBounds("tab//Game/Material.hasset").Z == 0 && !Window->ShouldClose(),
			               "Discard did not remove only its asset tab");
			AssetWorkspace->Open("/Game/Material.hasset");
			++Acceptance.ExerciseStep;
			break;
		case 172:
			if (Document && Document->Loaded().Path == "/Game/Material.hasset" && AssetWorkspace->IsPreviewReady())
			{
				CheckWorkspace(!Document->IsDirty() && ReadValue<std::string>(Document->Get("name")) ==
				                                           Acceptance.AssetExerciseOriginalName,
				               "Discard persisted an unsaved edit");
				Log(ELogLevel::Info, "Dirty asset discard and reopen passed");
				AssetWorkspace->Open("/Game/Texture.hasset");
				AssetWorkspace->Open("/Game/Sky.hasset");
				++Acceptance.ExerciseStep;
			}
			break;
	}
}

void FEditorPlugin::ExerciseAssetPanelInput(std::vector<FInputEvent>& InEvents)
{
	switch (Acceptance.ExerciseStep)
	{
		case 173:
			if (const auto* Document = AssetWorkspace->ActiveDocument();
			    Document && Document->Loaded().Path == "/Game/Sky.hasset" && AssetWorkspace->IsPreviewReady())
			{
				// Reproduce the user's floating panel, retaining the rest of the workspace layout.
				auto Layout = Gui->SaveLayout();
				const auto Start = Layout.find("[Window][Place Object]");
				const auto End = Layout.find("\n\n", Start);
				CheckWorkspace(Start != std::string::npos && End != std::string::npos,
				               "Place Object layout was not saved");
				Layout.replace(Start, End - Start, "[Window][Place Object]\nPos=400,200\nSize=300,500\nCollapsed=0");
				Gui->LoadLayout(Layout);
				bFocusPlacement = true;
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
			}
			break;
		case 174:
		{
			const auto Bounds = InspectionBounds.at("placement/title");
			Acceptance.AssetExercisePointer = {(Bounds.X + Bounds.Z) * .5f, (Bounds.Y + Bounds.W) * .5f};
			MovePointer(InEvents, Acceptance.AssetExercisePointer);
			if (++Acceptance.ExerciseWait == 3)
			{
				PointerButton(InEvents, Acceptance.AssetExercisePointer, true);
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
			}
			break;
		}
		case 175:
			// Observe sustained movement across multiple held-button frames.
			++Acceptance.ExerciseWait;
			if (Acceptance.ExerciseWait == 6)
			{
				Acceptance.AssetExercisePanelStart = InspectionBounds.at("placement/title");
			}
			if (Acceptance.ExerciseWait == 14)
			{
				const auto Bounds = InspectionBounds.at("placement/title");
				CheckWorkspace(std::abs(Bounds.X - Acceptance.AssetExercisePanelStart.X - 80) < 2 &&
				                   std::abs(Bounds.Y - Acceptance.AssetExercisePanelStart.Y - 48) < 2,
				               "Place Object window drag was interrupted while an asset tab was active");
				PointerButton(InEvents, Acceptance.AssetExercisePointer, false);
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
				break;
			}
			Acceptance.AssetExercisePointer.X += 10;
			Acceptance.AssetExercisePointer.Y += 6;
			MovePointer(InEvents, Acceptance.AssetExercisePointer);
			break;
		case 176:
			// A docking request from release is applied at the next frame boundary.
			if (++Acceptance.ExerciseWait < 3)
			{
				break;
			}
			CheckWorkspace(!Gui->PointerState().bDown, "Panel drag mouse release was not consumed");
			Acceptance.AssetExercisePanelStart = InspectionBounds.at("placement/title");
			MovePointer(InEvents, {Acceptance.AssetExercisePointer.X + 50, Acceptance.AssetExercisePointer.Y + 50});
			++Acceptance.ExerciseStep;
			break;
		case 177:
		{
			const auto Bounds = InspectionBounds.at("placement/title");
			CheckWorkspace(Bounds.X == Acceptance.AssetExercisePanelStart.X &&
			                   Bounds.Y == Acceptance.AssetExercisePanelStart.Y,
			               "Place Object window kept moving after mouse release");
			CheckWorkspace(!Placement.IsActive() && IsDirty() && History.size() == 1, "Panel drag changed the scene");
			Log(ELogLevel::Info, "Place Object panel held-button drag and release passed with three asset tabs");
			Acceptance.AssetExerciseIndex = 4;
			Acceptance.ExerciseWait = 0;
			AssetWorkspace->RevealProperty("field/name");
			Acceptance.ExerciseStep = 190;
			break;
		}
	}
}
} // namespace Hyperion
