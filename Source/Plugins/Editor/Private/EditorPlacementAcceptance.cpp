#include "EditorApplication.h"
#include <algorithm>

namespace Hyperion
{
namespace
{
constexpr std::array PlacementTypes{"Cube",       "Sphere",    "Cylinder", "Cone", "Plane", "DirectionalLight",
                                    "PointLight", "SpotLight", "SkyLight"};

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

void ClipboardKey(std::vector<FInputEvent>& InEvents, EKey InKey, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.bDown = bInDown;
	Event.Modifiers = bInDown ? 1 : 0;
	InEvents.push_back(Event);
}
} // namespace

bool FEditorPlugin::ExercisePlacementMenu(std::vector<FInputEvent>& InEvents)
{
	if (Acceptance.PlacementMenuStep > 8)
	{
		return true;
	}
	switch (Acceptance.PlacementMenuStep)
	{
		case 0:
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = true;
			InEvents.push_back(Focus);
			bShowPlacement = false;
			break;
		}
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
	++Acceptance.PlacementMenuStep;
	return false;
}

void FEditorPlugin::ExercisePlacementDrag(std::vector<FInputEvent>& InEvents)
{
	if (Acceptance.PlacementExerciseStep >= 13)
	{
		ExercisePlacedClipboard(InEvents);
		return;
	}
	const std::string Type = PlacementTypes.at(Acceptance.PlacementExerciseType);
	const auto Source = InspectionBounds.at("placement/" + Type);
	const auto Bounds = Viewport.ViewportRegion.Bounds;
	const FVec2 Target{Bounds.X + (Bounds.Z - Bounds.X) * (.2f + .18f * (Acceptance.PlacementExerciseType % 4)),
	                   Bounds.Y + (Bounds.W - Bounds.Y) * (.5f + .2f * (Acceptance.PlacementExerciseType / 4))};
	switch (Acceptance.PlacementExerciseStep)
	{
		case 0:
		{
			auto Candidate = Rendering;
			Candidate.bReversedZ = Acceptance.PlacementExerciseType % 2 != 0;
			SetRenderSettings(RenderSettingsRevision, Candidate);
			Acceptance.PlacementExerciseBaseNodes = Scene->GetNodes().size();
			Acceptance.PlacementExerciseBaseHistory = HistoryCursor;
			Acceptance.PlacementExerciseBaseState = DocumentState;
			Move(InEvents, Center(Source));
			break;
		}
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
			Check(Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes &&
			          HistoryCursor == Acceptance.PlacementExerciseBaseHistory &&
			          DocumentState == Acceptance.PlacementExerciseBaseState,
			      "preview modified document");
			Acceptance.PlacementExercisePosition = Placement.GetPreview()->Position;
			Acceptance.PlacementCapture = Options.ExercisePlacement.parent_path() / (Type + "Preview.png");
			Move(InEvents, {Target.X + 25, Target.Y + 12});
			break;
		case 8:
			Check(Placement.GetPreview() &&
			          Length(Subtract(Placement.GetPreview()->Position, Acceptance.PlacementExercisePosition)) > .01f,
			      Type + " preview did not follow pointer");
			Check(!PlacementRegistry.Find(Type)->Model || FreezePlacementPreview()->Items.size() == 1,
			      "missing mesh preview packet");
			break;
		case 10:
			Button(InEvents, false);
			break;
		case 12:
			Check(!Placement.IsActive() && bool(Selection), Type + " did not finish its drop");
			Check(Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes + 1 &&
			          HistoryCursor == Acceptance.PlacementExerciseBaseHistory + 1,
			      Type + " did not create exactly one transaction: " + Error);
			Acceptance.PlacementExerciseIds.push_back(Scene->FindNode(*Selection)->Id);
			if (Type == "DirectionalLight")
			{
				Check(Scene->GetLightingSelection().Directional.Handle == Selection.Primary(),
				      "first directional light did not become main");
			}
			if (Type == "SkyLight")
			{
				const auto& Light = Scene->FindNode(*Selection)->EnvironmentLight();
				Check(Light && Light->Source == ESceneEnvironmentSource::SkyAsset &&
				          Light->Sky.Path == DefaultSkyReference().Path,
				      "placed sky light does not reference the Engine default sky");
				Check(Scene->GetLightingSelection().Environment.Handle == Selection.Primary(),
				      "first sky light did not become active");
			}
			break;
	}
	++Acceptance.PlacementExerciseStep;
}

void FEditorPlugin::ExercisePlacedClipboard(std::vector<FInputEvent>& InEvents)
{
	const auto Original = Scene->FindHandle(Acceptance.PlacementExerciseIds.back());
	switch (Acceptance.PlacementExerciseStep)
	{
		case 13:
			// No extra click or FocusWindow: reproduce typing immediately after a real panel-to-viewport drop.
			ClipboardKey(InEvents, EKey::C, true);
			break;
		case 14:
			Check(Gui->IsWindowFocused("Viewport") && HistoryCursor == Acceptance.PlacementExerciseBaseHistory + 1,
			      "placed object did not retain scene shortcut focus or copy changed history: " + Error);
			ClipboardKey(InEvents, EKey::C, false);
			break;
		case 15:
			ClipboardKey(InEvents, EKey::V, true);
			break;
		case 16:
			ClipboardKey(InEvents, EKey::V, false);
			break;
		case 17:
		{
			Check(Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes + 2 &&
			          HistoryCursor == Acceptance.PlacementExerciseBaseHistory + 2 && Selection &&
			          *Selection != Original,
			      "Ctrl+V after placement did not create a separate selected object: " + Error);
			const auto* Source = Scene->FindNode(Original);
			auto Copy = *Scene->FindNode(*Selection);
			Check(Copy.Id != Source->Id && Copy.Name == Source->Name + " (1)", "placed copy identity/name mismatch");
			Copy.Id = Source->Id;
			Copy.Name = Source->Name;
			if (Copy.EnvironmentLight() && Source->EnvironmentLight())
			{
				// Prepared sky data is runtime state; the copy starts its own load.
				Copy.EnvironmentLight()->Data = Source->EnvironmentLight()->Data;
			}
			Check(Copy == *Source, "placed copy authored properties changed");
			if (Source->DirectionalLight())
			{
				Check(Scene->GetLightingSelection().Directional.bTied,
				      "copied directional priority tie was not reported");
			}
			const bool bSkyLight = Source->EnvironmentLight().has_value();
			if (bSkyLight)
			{
				Check(Scene->GetLightingSelection().Environment.bTied, "copied sky priority tie was not reported");
			}
			Undo();
			Check(Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes + 1 && Selection == Original,
			      "placed copy undo failed to restore selection");
			Undo();
			Check(Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes, "creation undo failed");
			Check(!bSkyLight || !Scene->GetLightingSelection().Environment.Handle,
			      "creation undo kept the active sky light");
			Redo();
			Check(Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes + 1, "creation redo failed");
			Check(!bSkyLight || Scene->GetLightingSelection().Environment.Handle ==
			                        Scene->FindHandle(Acceptance.PlacementExerciseIds.back()),
			      "creation redo did not restore the active sky light");
			Acceptance.PlacementExerciseStep = 0;
			++Acceptance.PlacementExerciseType;
			return;
		}
	}
	++Acceptance.PlacementExerciseStep;
}

void FEditorPlugin::ExercisePlacementCancel(std::vector<FInputEvent>& InEvents)
{
	const auto Source = InspectionBounds.at("placement/Cube");
	switch (Acceptance.PlacementExerciseStep)
	{
		case 0:
			if (Acceptance.PlacementCancelCase == 0)
			{
				Undo();
				Acceptance.PlacementExerciseBaseNodes = Scene->GetNodes().size();
				Acceptance.PlacementExerciseBaseHistory = HistoryCursor;
				Acceptance.PlacementExerciseBaseState = DocumentState;
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
			Move(InEvents, Center(Viewport.ViewportRegion.Bounds));
			break;
		case 6:
		{
			Check(Placement.GetPreview().has_value(), "cancellation setup has no preview");
			FInputEvent Event;
			Event.Type = Acceptance.PlacementCancelCase == 1 ? EEventType::Focus : EEventType::Key;
			Event.Key = EKey::Escape;
			Event.bDown = Acceptance.PlacementCancelCase != 1;
			if (Acceptance.PlacementCancelCase <= 1)
			{
				InEvents.push_back(Event);
			}
			else if (Acceptance.PlacementCancelCase == 2)
			{
				Move(InEvents, Center(Source));
			}
			else if (Acceptance.PlacementCancelCase == 3)
			{
				bOpenDialog = true;
			}
			else if (Acceptance.PlacementCancelCase == 4)
			{
				bShowViewport = false;
			}
			else if (Acceptance.PlacementCancelCase == 5)
			{
				Window->Resize({1440, 900});
			}
			else if (Acceptance.PlacementCancelCase == 6)
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
			Check(!Placement.IsActive() && Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes &&
			          DocumentState == Acceptance.PlacementExerciseBaseState &&
			          HistoryCursor == Acceptance.PlacementExerciseBaseHistory && History.size() == HistoryCursor + 2,
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
			++Acceptance.PlacementCancelCase;
			Acceptance.PlacementExerciseStep = 0;
			return;
	}
	++Acceptance.PlacementExerciseStep;
}

void FEditorPlugin::ExercisePlacementHistory()
{
	// Marker checks already restored the last creation; its copied object remains on the redo branch.
	Redo();
	Check(Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes + 2,
	      "paste redo did not survive cancelled gestures");
	Undo();
	Check(Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes + 1,
	      "paste undo after cancelled gestures failed");
	CommitPlacement(*PlacementRegistry.Find("DirectionalLight"), {3, 1, 0});
	const auto Additional = *Selection;
	const auto Previous = Scene->GetLightingSelection().Directional.Handle;
	auto Candidate = *Scene->FindNode(Additional);
	Candidate.DirectionalLight()->Priority = 10;
	CommitEdit(Additional, std::move(Candidate), Scene->GetRevision());
	Undo();
	Check(Scene->GetLightingSelection().Directional.Handle == Previous, "directional priority undo failed");
	Redo();
	Check(Scene->GetLightingSelection().Directional.Handle == Additional, "directional priority redo failed");
	ExercisePlacedSkyActivation();
	const auto Point = Scene->FindHandle(Acceptance.PlacementExerciseIds.at(6));
	FSceneNodeView View;
	Check(Scene->GetNodeView(Point, View), "placed point light disappeared");
	const auto Screen = ProjectViewportPoint(Viewport.ViewCamera, Viewport.ViewportRegion.Bounds,
	                                         {View.World.Values[12], View.World.Values[13], View.World.Values[14]});
	Check(Screen && PickLightMarker({Screen->X, Screen->Y}) == Point, "light marker hit test failed");
	bShowLightMarkers = false;
	Check(!PickLightMarker({Screen->X, Screen->Y}), "hidden markers remained pickable");
	bShowLightMarkers = true;
	Scene->SetEnabled(Point, false);
	Check(!PickLightMarker({Screen->X, Screen->Y}), "disabled light remained pickable");
	Scene->SetEnabled(Point, true);
	Acceptance.PlacementExerciseBaseNodes = Scene->GetNodes().size();
}

void FEditorPlugin::ExercisePlacedSkyActivation()
{
	const auto Active = Scene->GetLightingSelection().Environment.Handle;
	Check(Active && Scene->FindNode(*Active), "placed sky light is not active");
	CommitPlacement(*PlacementRegistry.Find("SkyLight"), {-3, 1, 0});
	const auto Additional = *Selection;
	Check(Additional != *Active && Scene->FindNode(Additional)->EnvironmentLight(), "second sky light was not placed");
	const auto Previous = Scene->GetLightingSelection().Environment.Handle;
	auto Candidate = *Scene->FindNode(Additional);
	Candidate.EnvironmentLight()->Priority = 10;
	CommitEdit(Additional, std::move(Candidate), Scene->GetRevision());
	Check(Scene->GetLightingSelection().Environment.Handle == Additional, "sky priority selection failed");
	Undo();
	Check(Scene->GetLightingSelection().Environment.Handle == Previous, "sky priority undo failed");
	Redo();
	Check(Scene->GetLightingSelection().Environment.Handle == Additional, "sky priority redo failed");
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
	if (FrameCount < 12 || (!Viewport.bViewportVisible && Acceptance.PlacementExerciseStep == 0) ||
	    !Scene->GetStatus().bReady || PlacementModels.size() != 5)
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
	if (Acceptance.PlacementExerciseType < PlacementTypes.size())
	{
		ExercisePlacementDrag(InEvents);
		return;
	}
	if (Acceptance.PlacementCancelCase < 8)
	{
		ExercisePlacementCancel(InEvents);
		return;
	}
	if (Acceptance.PlacementMarkerCase < 3)
	{
		ExercisePlacementMarkers(InEvents);
		return;
	}
	ExercisePlacementDocument(InEvents);
}

void FEditorPlugin::ExercisePlacementDocument(std::vector<FInputEvent>& InEvents)
{
	if (Acceptance.PlacementExerciseStep == 0)
	{
		ExercisePlacementHistory();
		Scene->Tick();
		Acceptance.PlacementExerciseBaseHistory = HistoryCursor;
		SelectObject(std::nullopt);
		++Acceptance.PlacementExerciseStep;
	}
	else if (Acceptance.PlacementExerciseStep == 1)
	{
		FSceneNodeView View;
		Check(Scene->GetNodeView(Scene->FindHandle(Acceptance.PlacementExerciseIds.at(6)), View),
		      "point light unavailable");
		const auto Screen = ProjectViewportPoint(Viewport.ViewCamera, Viewport.ViewportRegion.Bounds,
		                                         {View.World.Values[12], View.World.Values[13], View.World.Values[14]});
		Check(Screen.has_value(), "point light is outside viewport");
		Move(InEvents, {Screen->X, Screen->Y});
		++Acceptance.PlacementExerciseStep;
	}
	else if (Acceptance.PlacementExerciseStep == 2 || Acceptance.PlacementExerciseStep == 3)
	{
		Button(InEvents, Acceptance.PlacementExerciseStep == 2);
		++Acceptance.PlacementExerciseStep;
	}
	else if (Acceptance.PlacementExerciseStep == 4)
	{
		Check(Selection == Scene->FindHandle(Acceptance.PlacementExerciseIds.at(6)) &&
		          HistoryCursor == Acceptance.PlacementExerciseBaseHistory,
		      "clicking light marker did not select it without editing history");
		SaveScene(Options.ExercisePlacement.generic_string());
		++Acceptance.PlacementExerciseStep;
	}
	else if (Acceptance.PlacementExerciseStep == 5 && !PendingSave)
	{
		Check(!IsDirty(), "save did not complete");
		OpenScene(Options.ExercisePlacement.generic_string());
		++Acceptance.PlacementExerciseStep;
	}
	else if (Acceptance.PlacementExerciseStep == 6)
	{
		Check(Scene->GetNodes().size() == Acceptance.PlacementExerciseBaseNodes, "saved placed objects did not reload");
		for (const auto& Id : Acceptance.PlacementExerciseIds)
		{
			Check(Scene->FindHandle(Id).Scene != 0, "placed object missing after reload: " + Id);
		}
		Check(Scene->GetLightingSelection().Directional.Handle.has_value(), "main light selection did not persist");
		const auto Sky = Scene->GetLightingSelection().Environment.Handle;
		Check(Sky && Scene->FindNode(*Sky) && Scene->FindNode(*Sky)->EnvironmentLight() &&
		          *Sky != Scene->FindHandle(Acceptance.PlacementExerciseIds.at(8)),
		      "sky priority did not persist");
		Acceptance.bPlacementVerified = true;
	}
}
} // namespace Hyperion
