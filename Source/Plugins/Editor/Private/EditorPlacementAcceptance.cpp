#include "EditorAcceptanceHarness.h"
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

bool FEditorAcceptanceHarness::ExercisePlacementMenu(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.PlacementMenuStep > 8)
	{
		return true;
	}
	switch (Scenario.PlacementMenuStep)
	{
		case 0:
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = true;
			InEvents.push_back(Focus);
			Editor.bShowPlacement = false;
			break;
		}
		case 1:
			Move(InEvents, Center(Scenario.InspectionBounds.at("placement/window-menu")));
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
			Move(InEvents, Center(Scenario.InspectionBounds.at("placement/open-panel")));
			break;
		case 8:
			Check(Editor.bShowPlacement, "Window > Place Object did not reopen the panel");
			break;
	}
	++Scenario.PlacementMenuStep;
	return false;
}

void FEditorAcceptanceHarness::ExercisePlacementDrag(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.PlacementExerciseStep >= 13)
	{
		ExercisePlacedClipboard(InEvents);
		return;
	}
	const std::string Type = PlacementTypes.at(Scenario.PlacementExerciseType);
	const auto Source = Scenario.InspectionBounds.at("placement/" + Type);
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Target{Bounds.X + (Bounds.Z - Bounds.X) * (.2f + .18f * (Scenario.PlacementExerciseType % 4)),
	                   Bounds.Y + (Bounds.W - Bounds.Y) * (.5f + .2f * (Scenario.PlacementExerciseType / 4))};
	switch (Scenario.PlacementExerciseStep)
	{
		case 0:
		{
			auto Candidate = Editor.Rendering;
			Candidate.bReversedZ = Scenario.PlacementExerciseType % 2 != 0;
			Editor.SetRenderSettings(Editor.RenderSettingsRevision, Candidate);
			Scenario.PlacementExerciseBaseNodes = Editor.Scene->GetNodes().size();
			Scenario.PlacementExerciseBaseHistory = Editor.HistoryCursor;
			Scenario.PlacementExerciseBaseState = Editor.DocumentState;
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
			Check(Editor.Gui->DragPayload().has_value(), Type + " source did not start dragging");
			Move(InEvents, Target);
			break;
		case 6:
			Check(Editor.Placement.GetPreview().has_value(), Type + " has no world preview: " + Editor.PlacementStatus);
			Check(Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes &&
			          Editor.HistoryCursor == Scenario.PlacementExerciseBaseHistory &&
			          Editor.DocumentState == Scenario.PlacementExerciseBaseState,
			      "preview modified document");
			Scenario.PlacementExercisePosition = Editor.Placement.GetPreview()->Position;
			Scenario.PlacementCapture = Editor.Options.ExercisePlacement.parent_path() / (Type + "Preview.png");
			Move(InEvents, {Target.X + 25, Target.Y + 12});
			break;
		case 8:
			Check(Editor.Placement.GetPreview() && Length(Subtract(Editor.Placement.GetPreview()->Position,
			                                                       Scenario.PlacementExercisePosition)) > .01f,
			      Type + " preview did not follow pointer");
			Check(!Editor.PlacementRegistry.Find(Type)->Model || Editor.FreezePlacementPreview()->Items.size() == 1,
			      "missing mesh preview packet");
			break;
		case 10:
			Button(InEvents, false);
			break;
		case 12:
			Check(!Editor.Placement.IsActive() && bool(Editor.Selection), Type + " did not finish its drop");
			Check(Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes + 1 &&
			          Editor.HistoryCursor == Scenario.PlacementExerciseBaseHistory + 1,
			      Type + " did not create exactly one transaction: " + Editor.Error);
			Scenario.PlacementExerciseIds.push_back(Editor.Scene->FindNode(*Editor.Selection)->Id);
			if (Type == "DirectionalLight")
			{
				Check(Editor.Scene->GetLightingSelection().Directional.Handle == Editor.Selection.Primary(),
				      "first directional light did not become main");
			}
			if (Type == "SkyLight")
			{
				const auto& Light = Editor.Scene->FindNode(*Editor.Selection)->EnvironmentLight();
				Check(Light && Light->Source == ESceneEnvironmentSource::SkyAsset &&
				          Light->Sky.Path == DefaultSkyReference().Path,
				      "placed sky light does not reference the Engine default sky");
				Check(Editor.Scene->GetLightingSelection().Environment.Handle == Editor.Selection.Primary(),
				      "first sky light did not become active");
			}
			break;
	}
	++Scenario.PlacementExerciseStep;
}

void FEditorAcceptanceHarness::ExercisePlacedClipboard(std::vector<FInputEvent>& InEvents)
{
	const auto Original = Editor.Scene->FindHandle(Scenario.PlacementExerciseIds.back());
	switch (Scenario.PlacementExerciseStep)
	{
		case 13:
			// No extra click or FocusWindow: reproduce typing immediately after a real panel-to-viewport drop.
			ClipboardKey(InEvents, EKey::C, true);
			break;
		case 14:
			Check(Editor.Gui->IsWindowFocused("Viewport") &&
			          Editor.HistoryCursor == Scenario.PlacementExerciseBaseHistory + 1,
			      "placed object did not retain scene shortcut focus or copy changed history: " + Editor.Error);
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
			Check(Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes + 2 &&
			          Editor.HistoryCursor == Scenario.PlacementExerciseBaseHistory + 2 && Editor.Selection &&
			          *Editor.Selection != Original,
			      "Ctrl+V after placement did not create a separate selected object: " + Editor.Error);
			const auto* Source = Editor.Scene->FindNode(Original);
			auto Copy = *Editor.Scene->FindNode(*Editor.Selection);
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
				Check(Editor.Scene->GetLightingSelection().Directional.bTied,
				      "copied directional priority tie was not reported");
			}
			const bool bSkyLight = Source->EnvironmentLight().has_value();
			if (bSkyLight)
			{
				Check(Editor.Scene->GetLightingSelection().Environment.bTied,
				      "copied sky priority tie was not reported");
			}
			Editor.Undo();
			Check(Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes + 1 &&
			          Editor.Selection == Original,
			      "placed copy undo failed to restore selection");
			Editor.Undo();
			Check(Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes, "creation undo failed");
			Check(!bSkyLight || !Editor.Scene->GetLightingSelection().Environment.Handle,
			      "creation undo kept the active sky light");
			Editor.Redo();
			Check(Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes + 1, "creation redo failed");
			Check(!bSkyLight || Editor.Scene->GetLightingSelection().Environment.Handle ==
			                        Editor.Scene->FindHandle(Scenario.PlacementExerciseIds.back()),
			      "creation redo did not restore the active sky light");
			Scenario.PlacementExerciseStep = 0;
			++Scenario.PlacementExerciseType;
			return;
		}
	}
	++Scenario.PlacementExerciseStep;
}

void FEditorAcceptanceHarness::ExercisePlacementCancel(std::vector<FInputEvent>& InEvents)
{
	const auto Source = Scenario.InspectionBounds.at("placement/Cube");
	switch (Scenario.PlacementExerciseStep)
	{
		case 0:
			if (Scenario.PlacementCancelCase == 0)
			{
				Editor.Undo();
				Scenario.PlacementExerciseBaseNodes = Editor.Scene->GetNodes().size();
				Scenario.PlacementExerciseBaseHistory = Editor.HistoryCursor;
				Scenario.PlacementExerciseBaseState = Editor.DocumentState;
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
			Move(InEvents, Center(Editor.Viewport.ViewportRegion.Bounds));
			break;
		case 6:
		{
			Check(Editor.Placement.GetPreview().has_value(), "cancellation setup has no preview");
			FInputEvent Event;
			Event.Type = Scenario.PlacementCancelCase == 1 ? EEventType::Focus : EEventType::Key;
			Event.Key = EKey::Escape;
			Event.bDown = Scenario.PlacementCancelCase != 1;
			if (Scenario.PlacementCancelCase <= 1)
			{
				InEvents.push_back(Event);
			}
			else if (Scenario.PlacementCancelCase == 2)
			{
				Move(InEvents, Center(Source));
			}
			else if (Scenario.PlacementCancelCase == 3)
			{
				Editor.bOpenDialog = true;
			}
			else if (Scenario.PlacementCancelCase == 4)
			{
				Editor.bShowViewport = false;
			}
			else if (Scenario.PlacementCancelCase == 5)
			{
				Editor.Window->Resize({1440, 900});
			}
			else if (Scenario.PlacementCancelCase == 6)
			{
				Editor.Gui->SetApplicationScale(1.5f);
			}
			else
			{
				Editor.SceneDocument.Invalidate();
			}
			break;
		}
		case 7:
			Button(InEvents, false);
			break;
		case 9:
		{
			Check(!Editor.Placement.IsActive() &&
			          Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes &&
			          Editor.DocumentState == Scenario.PlacementExerciseBaseState &&
			          Editor.HistoryCursor == Scenario.PlacementExerciseBaseHistory &&
			          Editor.History.size() == Editor.HistoryCursor + 2,
			      "cancel changed document or redo branch");
			FInputEvent Event;
			Event.Type = EEventType::Key;
			Event.Key = EKey::Escape;
			InEvents.push_back(Event);
			Event.Type = EEventType::Focus;
			Event.bDown = true;
			InEvents.push_back(Event);
			Editor.bOpenDialog = false;
			Editor.bShowViewport = true;
			break;
		}
		case 14:
			++Scenario.PlacementCancelCase;
			Scenario.PlacementExerciseStep = 0;
			return;
	}
	++Scenario.PlacementExerciseStep;
}

void FEditorAcceptanceHarness::ExercisePlacementHistory()
{
	// Marker checks already restored the last creation; its copied object remains on the redo branch.
	Editor.Redo();
	Check(Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes + 2,
	      "paste redo did not survive cancelled gestures");
	Editor.Undo();
	Check(Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes + 1,
	      "paste undo after cancelled gestures failed");
	Editor.CommitPlacement(*Editor.PlacementRegistry.Find("DirectionalLight"), {3, 1, 0});
	const auto Additional = *Editor.Selection;
	const auto Previous = Editor.Scene->GetLightingSelection().Directional.Handle;
	auto Candidate = *Editor.Scene->FindNode(Additional);
	Candidate.DirectionalLight()->Priority = 10;
	Editor.CommitEdit(Additional, std::move(Candidate), Editor.Scene->GetRevision());
	Editor.Undo();
	Check(Editor.Scene->GetLightingSelection().Directional.Handle == Previous, "directional priority undo failed");
	Editor.Redo();
	Check(Editor.Scene->GetLightingSelection().Directional.Handle == Additional, "directional priority redo failed");
	ExercisePlacedSkyActivation();
	const auto Point = Editor.Scene->FindHandle(Scenario.PlacementExerciseIds.at(6));
	FSceneNodeView View;
	Check(Editor.Scene->GetNodeView(Point, View), "placed point light disappeared");
	const auto Screen = ProjectViewportPoint(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportRegion.Bounds,
	                                         {View.World.Values[12], View.World.Values[13], View.World.Values[14]});
	Check(Screen && Editor.PickLightMarker({Screen->X, Screen->Y}) == Point, "light marker hit test failed");
	Editor.bShowLightMarkers = false;
	Check(!Editor.PickLightMarker({Screen->X, Screen->Y}), "hidden markers remained pickable");
	Editor.bShowLightMarkers = true;
	Editor.Scene->SetEnabled(Point, false);
	Check(!Editor.PickLightMarker({Screen->X, Screen->Y}), "disabled light remained pickable");
	Editor.Scene->SetEnabled(Point, true);
	Scenario.PlacementExerciseBaseNodes = Editor.Scene->GetNodes().size();
}

void FEditorAcceptanceHarness::ExercisePlacedSkyActivation()
{
	const auto Active = Editor.Scene->GetLightingSelection().Environment.Handle;
	Check(Active && Editor.Scene->FindNode(*Active), "placed sky light is not active");
	Editor.CommitPlacement(*Editor.PlacementRegistry.Find("SkyLight"), {-3, 1, 0});
	const auto Additional = *Editor.Selection;
	Check(Additional != *Active && Editor.Scene->FindNode(Additional)->EnvironmentLight(),
	      "second sky light was not placed");
	const auto Previous = Editor.Scene->GetLightingSelection().Environment.Handle;
	auto Candidate = *Editor.Scene->FindNode(Additional);
	Candidate.EnvironmentLight()->Priority = 10;
	Editor.CommitEdit(Additional, std::move(Candidate), Editor.Scene->GetRevision());
	Check(Editor.Scene->GetLightingSelection().Environment.Handle == Additional, "sky priority selection failed");
	Editor.Undo();
	Check(Editor.Scene->GetLightingSelection().Environment.Handle == Previous, "sky priority undo failed");
	Editor.Redo();
	Check(Editor.Scene->GetLightingSelection().Environment.Handle == Additional, "sky priority redo failed");
}

void FEditorAcceptanceHarness::ExercisePlacementInput(std::vector<FInputEvent>& InEvents)
{
	if (Editor.FrameCount < 3 || !ExercisePlacementMenu(InEvents))
	{
		return;
	}
	Check(Editor.Scene->GetStatus().Error.empty(), Editor.Scene->GetStatus().Error);
	for (const auto Handle : Editor.Scene->GetHandles())
	{
		Check(Editor.Scene->GetError(Handle).empty(), Editor.Scene->GetError(Handle));
	}
	if (Editor.FrameCount < 12 || (!Editor.Viewport.bViewportVisible && Scenario.PlacementExerciseStep == 0) ||
	    !Editor.Scene->GetStatus().bReady || Editor.PlacementModels.size() != 5)
	{
		return;
	}
	for (const auto& [Id, Model] : Editor.PlacementModels)
	{
		Check(Model.Error.empty(), Model.Error);
		if (!Model.Resource || Model.Resource->GetStatus() != ERenderResourceStatus::Ready)
		{
			return;
		}
	}
	Check(Editor.PlacementMaterial->GetStatus() != ERenderMaterialStatus::Failed, Editor.PlacementMaterial->GetError());
	if (Editor.PlacementMaterial->GetStatus() != ERenderMaterialStatus::Ready ||
	    std::any_of(Editor.PlacementIcons.begin(), Editor.PlacementIcons.end(),
	                [](const auto& InEntry)
	                {
		                return !InEntry.second.Source.Texture;
	                }))
	{
		return;
	}
	if (Scenario.PlacementExerciseType < PlacementTypes.size())
	{
		ExercisePlacementDrag(InEvents);
		return;
	}
	if (Scenario.PlacementCancelCase < 8)
	{
		ExercisePlacementCancel(InEvents);
		return;
	}
	if (Scenario.PlacementMarkerCase < 3)
	{
		ExercisePlacementMarkers(InEvents);
		return;
	}
	ExercisePlacementDocument(InEvents);
}

void FEditorAcceptanceHarness::ExercisePlacementDocument(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.PlacementExerciseStep == 0)
	{
		ExercisePlacementHistory();
		Editor.Scene->Tick();
		Scenario.PlacementExerciseBaseHistory = Editor.HistoryCursor;
		Editor.SelectObject(std::nullopt);
		++Scenario.PlacementExerciseStep;
	}
	else if (Scenario.PlacementExerciseStep == 1)
	{
		FSceneNodeView View;
		Check(Editor.Scene->GetNodeView(Editor.Scene->FindHandle(Scenario.PlacementExerciseIds.at(6)), View),
		      "point light unavailable");
		const auto Screen = ProjectViewportPoint(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportRegion.Bounds,
		                                         {View.World.Values[12], View.World.Values[13], View.World.Values[14]});
		Check(Screen.has_value(), "point light is outside viewport");
		Move(InEvents, {Screen->X, Screen->Y});
		++Scenario.PlacementExerciseStep;
	}
	else if (Scenario.PlacementExerciseStep == 2 || Scenario.PlacementExerciseStep == 3)
	{
		Button(InEvents, Scenario.PlacementExerciseStep == 2);
		++Scenario.PlacementExerciseStep;
	}
	else if (Scenario.PlacementExerciseStep == 4)
	{
		Check(Editor.Selection == Editor.Scene->FindHandle(Scenario.PlacementExerciseIds.at(6)) &&
		          Editor.HistoryCursor == Scenario.PlacementExerciseBaseHistory,
		      "clicking light marker did not select it without editing history");
		Editor.SaveScene(Editor.Options.ExercisePlacement.generic_string());
		++Scenario.PlacementExerciseStep;
	}
	else if (Scenario.PlacementExerciseStep == 5 && !Editor.PendingSave)
	{
		Check(!Editor.IsDirty(), "save did not complete");
		Editor.OpenScene(Editor.Options.ExercisePlacement.generic_string());
		++Scenario.PlacementExerciseStep;
	}
	else if (Scenario.PlacementExerciseStep == 6)
	{
		Check(Editor.Scene->GetNodes().size() == Scenario.PlacementExerciseBaseNodes,
		      "saved placed objects did not reload");
		for (const auto& Id : Scenario.PlacementExerciseIds)
		{
			Check(Editor.Scene->FindHandle(Id).Scene != 0, "placed object missing after reload: " + Id);
		}
		Check(Editor.Scene->GetLightingSelection().Directional.Handle.has_value(),
		      "main light selection did not persist");
		const auto Sky = Editor.Scene->GetLightingSelection().Environment.Handle;
		Check(Sky && Editor.Scene->FindNode(*Sky) && Editor.Scene->FindNode(*Sky)->EnvironmentLight() &&
		          *Sky != Editor.Scene->FindHandle(Scenario.PlacementExerciseIds.at(8)),
		      "sky priority did not persist");
		Scenario.bPlacementVerified = true;
	}
}
} // namespace Hyperion
