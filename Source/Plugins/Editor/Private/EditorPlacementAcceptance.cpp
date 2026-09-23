#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
constexpr std::array PlacementTypes{"Cube",  "Sphere",           "Cylinder",   "Cone",
                                    "Plane", "DirectionalLight", "PointLight", "SpotLight"};

void Check(bool bInCondition, const std::string& InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error("Placement acceptance: " + InMessage);
	}
}

void Move(std::vector<FInputEvent>& InEvents, FVec2 InPoint)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
}

void Button(std::vector<FInputEvent>& InEvents, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseButton;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}

FVec2 Center(FVec4 InBounds)
{
	return {(InBounds.X + InBounds.Z) / 2, (InBounds.Y + InBounds.W) / 2};
}
} // namespace

bool FEditorPlugin::ExercisePlacementMenu(std::vector<FInputEvent>& InEvents)
{
	if (PlacementMenuStep > 8)
	{
		return true;
	}
	switch (PlacementMenuStep)
	{
		case 0:
			bShowPlacement = false;
			break;
		case 1:
			Move(InEvents, Center(InspectionBounds.at("placement/window-menu")));
			break;
		case 2:
		case 5:
			Button(InEvents, true);
			break;
		case 3:
		case 6:
			Button(InEvents, false);
			break;
		case 4:
			Move(InEvents, Center(InspectionBounds.at("placement/open-panel")));
			break;
		case 8:
			Check(bShowPlacement, "Window > Place Object did not reopen the panel");
			break;
	}
	++PlacementMenuStep;
	return false;
}

void FEditorPlugin::ExercisePlacementDrag(std::vector<FInputEvent>& InEvents)
{
	const std::string Type = PlacementTypes.at(PlacementExerciseType);
	const auto Source = InspectionBounds.at("placement/" + Type);
	const auto Bounds = ViewportRegion.Bounds;
	const FVec2 Target{Bounds.X + (Bounds.Z - Bounds.X) * (.2f + .18f * (PlacementExerciseType % 4)),
	                   Bounds.Y + (Bounds.W - Bounds.Y) * (.53f + .22f * (PlacementExerciseType / 4))};
	switch (PlacementExerciseStep)
	{
		case 0:
			PlacementExerciseBaseNodes = Scene->GetNodes().size();
			PlacementExerciseBaseHistory = HistoryCursor;
			PlacementExerciseBaseState = DocumentState;
			Move(InEvents, Center(Source));
			break;
		case 1:
			Button(InEvents, true);
			break;
		case 2:
			Move(InEvents, {Center(Source).X + 15, Center(Source).Y});
			break;
		case 3:
			Check(Gui->DragPayload().has_value(), Type + " source did not start dragging");
			Move(InEvents, Target);
			break;
		case 6:
			Check(Placement.GetPreview().has_value(), Type + " has no world preview: " + PlacementStatus);
			Check(Scene->GetNodes().size() == PlacementExerciseBaseNodes &&
			          HistoryCursor == PlacementExerciseBaseHistory && DocumentState == PlacementExerciseBaseState,
			      "preview modified document");
			PlacementExercisePosition = Placement.GetPreview()->Position;
			PlacementCapture = Options.ExercisePlacement.parent_path() / (Type + "Preview.png");
			Move(InEvents, {Target.X + 25, Target.Y + 12});
			break;
		case 8:
			Check(Placement.GetPreview() &&
			          Length(Subtract(Placement.GetPreview()->Position, PlacementExercisePosition)) > .01f,
			      Type + " preview did not follow pointer");
			Check(!PlacementRegistry.Find(Type)->Model || FreezePlacementPreview()->Items.size() == 1,
			      "missing mesh preview packet");
			break;
		case 10:
			Button(InEvents, false);
			break;
		case 12:
			Check(!Placement.IsActive() && bool(Selection), Type + " did not finish its drop");
			Check(Scene->GetNodes().size() == PlacementExerciseBaseNodes + 1 &&
			          HistoryCursor == PlacementExerciseBaseHistory + 1,
			      Type + " did not create exactly one transaction: " + Error);
			PlacementExerciseIds.push_back(Scene->FindNode(*Selection)->Id);
			if (Type == "DirectionalLight")
			{
				Check(Scene->GetSettings().MainDirectionalLight == Selection.Primary(),
				      "first directional light did not become main");
			}
			Undo();
			Check(Scene->GetNodes().size() == PlacementExerciseBaseNodes, "creation undo failed");
			Redo();
			Check(Scene->GetNodes().size() == PlacementExerciseBaseNodes + 1, "creation redo failed");
			PlacementExerciseStep = 0;
			++PlacementExerciseType;
			return;
	}
	++PlacementExerciseStep;
}

void FEditorPlugin::ExercisePlacementCancel(std::vector<FInputEvent>& InEvents)
{
	const auto Source = InspectionBounds.at("placement/Cube");
	switch (PlacementExerciseStep)
	{
		case 0:
			if (PlacementCancelCase == 0)
			{
				Undo();
				PlacementExerciseBaseNodes = Scene->GetNodes().size();
				PlacementExerciseBaseHistory = HistoryCursor;
				PlacementExerciseBaseState = DocumentState;
			}
			Move(InEvents, Center(Source));
			break;
		case 1:
			Button(InEvents, true);
			break;
		case 2:
			Move(InEvents, {Center(Source).X + 15, Center(Source).Y});
			break;
		case 3:
			Move(InEvents, Center(ViewportRegion.Bounds));
			break;
		case 6:
		{
			Check(Placement.GetPreview().has_value(), "cancellation setup has no preview");
			FInputEvent Event;
			Event.Type = PlacementCancelCase == 1 ? EEventType::Focus : EEventType::Key;
			Event.Key = EKey::Escape;
			Event.bDown = PlacementCancelCase != 1;
			if (PlacementCancelCase <= 1)
			{
				InEvents.push_back(Event);
			}
			else if (PlacementCancelCase == 2)
			{
				Move(InEvents, Center(Source));
			}
			else if (PlacementCancelCase == 3)
			{
				bOpenDialog = true;
			}
			else if (PlacementCancelCase == 4)
			{
				bShowViewport = false;
			}
			else if (PlacementCancelCase == 5)
			{
				Window->Resize({1440, 900});
			}
			else if (PlacementCancelCase == 6)
			{
				Gui->SetApplicationScale(1.5f);
			}
			else
			{
				SceneDocument.Invalidate();
			}
			break;
		}
		case 7:
			Button(InEvents, false);
			break;
		case 9:
		{
			Check(!Placement.IsActive() && Scene->GetNodes().size() == PlacementExerciseBaseNodes &&
			          DocumentState == PlacementExerciseBaseState && HistoryCursor == PlacementExerciseBaseHistory &&
			          History.size() == HistoryCursor + 1,
			      "cancel changed document or redo branch");
			FInputEvent Event;
			Event.Type = EEventType::Key;
			Event.Key = EKey::Escape;
			InEvents.push_back(Event);
			Event.Type = EEventType::Focus;
			Event.bDown = true;
			InEvents.push_back(Event);
			bOpenDialog = false;
			bShowViewport = true;
			break;
		}
		case 14:
			++PlacementCancelCase;
			PlacementExerciseStep = 0;
			return;
	}
	++PlacementExerciseStep;
}

void FEditorPlugin::ExercisePlacementHistory()
{
	Redo();
	Check(Scene->GetNodes().size() == PlacementExerciseBaseNodes + 1, "redo did not survive cancelled gestures");
	const auto Main = Scene->GetSettings().MainDirectionalLight;
	CommitPlacement(*PlacementRegistry.Find("DirectionalLight"), {3, 1, 0});
	const auto Additional = *Selection;
	Check(Scene->GetSettings().MainDirectionalLight == Main, "second directional light replaced main implicitly");
	auto Settings = Scene->GetSettings();
	Settings.MainDirectionalLight = Additional;
	CommitSettings(Settings);
	Undo();
	Check(Scene->GetSettings().MainDirectionalLight == Main, "main-light action undo failed");
	Redo();
	Check(Scene->GetSettings().MainDirectionalLight == Additional, "main-light action redo failed");
	const auto Point = Scene->FindHandle(PlacementExerciseIds.at(6));
	FSceneNodeView View;
	Check(Scene->GetNodeView(Point, View), "placed point light disappeared");
	const auto Screen = ProjectViewportPoint(ViewCamera, ViewportRegion.Bounds,
	                                         {View.World.Values[12], View.World.Values[13], View.World.Values[14]});
	Check(Screen && PickLightMarker({Screen->X, Screen->Y}) == Point, "light marker hit test failed");
	bShowLightMarkers = false;
	Check(!PickLightMarker({Screen->X, Screen->Y}), "hidden markers remained pickable");
	bShowLightMarkers = true;
	Scene->SetEnabled(Point, false);
	Check(!PickLightMarker({Screen->X, Screen->Y}), "disabled light remained pickable");
	Scene->SetEnabled(Point, true);
	PlacementExerciseBaseNodes = Scene->GetNodes().size();
}

void FEditorPlugin::ExercisePlacementInput(std::vector<FInputEvent>& InEvents)
{
	if (FrameCount < 3 || !ExercisePlacementMenu(InEvents))
	{
		return;
	}
	Check(Scene->GetStatus().Error.empty(), Scene->GetStatus().Error);
	for (const auto Handle : Scene->GetHandles())
	{
		Check(Scene->GetError(Handle).empty(), Scene->GetError(Handle));
	}
	if (FrameCount < 12 || (!bViewportVisible && PlacementExerciseStep == 0) || !Scene->GetStatus().bReady ||
	    PlacementModels.size() != 5)
	{
		return;
	}
	for (const auto& [Id, Model] : PlacementModels)
	{
		Check(Model.Error.empty(), Model.Error);
		if (!Model.Resource || Model.Resource->GetStatus() != ERenderResourceStatus::Ready)
		{
			return;
		}
	}
	Check(PlacementMaterial->GetStatus() != ERenderMaterialStatus::Failed, PlacementMaterial->GetError());
	if (PlacementMaterial->GetStatus() != ERenderMaterialStatus::Ready ||
	    std::any_of(PlacementIcons.begin(), PlacementIcons.end(),
	                [](const auto& InEntry)
	                {
		                return !InEntry.second.Source.Texture;
	                }))
	{
		return;
	}
	if (PlacementExerciseType < PlacementTypes.size())
	{
		ExercisePlacementDrag(InEvents);
		return;
	}
	if (PlacementCancelCase < 8)
	{
		ExercisePlacementCancel(InEvents);
		return;
	}
	if (PlacementMarkerCase < 3)
	{
		ExercisePlacementMarkers(InEvents);
		return;
	}
	ExercisePlacementDocument(InEvents);
}

void FEditorPlugin::ExercisePlacementDocument(std::vector<FInputEvent>& InEvents)
{
	if (PlacementExerciseStep == 0)
	{
		ExercisePlacementHistory();
		Scene->Tick();
		PlacementExerciseBaseHistory = HistoryCursor;
		SelectObject(std::nullopt);
		++PlacementExerciseStep;
	}
	else if (PlacementExerciseStep == 1)
	{
		FSceneNodeView View;
		Check(Scene->GetNodeView(Scene->FindHandle(PlacementExerciseIds.at(6)), View), "point light unavailable");
		const auto Screen = ProjectViewportPoint(ViewCamera, ViewportRegion.Bounds,
		                                         {View.World.Values[12], View.World.Values[13], View.World.Values[14]});
		Check(Screen.has_value(), "point light is outside viewport");
		Move(InEvents, {Screen->X, Screen->Y});
		++PlacementExerciseStep;
	}
	else if (PlacementExerciseStep == 2 || PlacementExerciseStep == 3)
	{
		Button(InEvents, PlacementExerciseStep == 2);
		++PlacementExerciseStep;
	}
	else if (PlacementExerciseStep == 4)
	{
		Check(Selection == Scene->FindHandle(PlacementExerciseIds.at(6)) &&
		          HistoryCursor == PlacementExerciseBaseHistory,
		      "clicking light marker did not select it without editing history");
		SaveScene(Options.ExercisePlacement.generic_string());
		++PlacementExerciseStep;
	}
	else if (PlacementExerciseStep == 5 && !PendingSave)
	{
		Check(!IsDirty(), "save did not complete");
		OpenScene(Options.ExercisePlacement.generic_string());
		++PlacementExerciseStep;
	}
	else if (PlacementExerciseStep == 6)
	{
		Check(Scene->GetNodes().size() == PlacementExerciseBaseNodes, "saved placed objects did not reload");
		for (const auto& Id : PlacementExerciseIds)
		{
			Check(Scene->FindHandle(Id).Scene != 0, "placed object missing after reload: " + Id);
		}
		Check(Scene->GetSettings().MainDirectionalLight.has_value(), "main light selection did not persist");
		bPlacementVerified = true;
	}
}
} // namespace Hyperion
