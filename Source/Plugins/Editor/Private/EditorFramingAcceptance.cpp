#include "EditorApplication.h"
#include "Hyperion/Renderer/SceneNavigation.h"
#include "Hyperion/Renderer/ViewportRay.h"
#include <cmath>

namespace Hyperion
{
namespace
{
void RequireFraming(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(std::string("Framing acceptance: ") + InMessage);
	}
}

void FramingKey(std::vector<FInputEvent>& InEvents, EKey InKey = EKey::F, bool bInDown = true, bool bInRepeat = false,
                unsigned InModifiers = 0)
{
	FInputEvent Event;
	Event.Type = EEventType::Key;
	Event.Key = InKey;
	Event.bDown = bInDown;
	Event.bRepeat = bInRepeat;
	Event.Modifiers = InModifiers;
	InEvents.push_back(Event);
}

void FramingClick(std::vector<FInputEvent>& InEvents, FVec2 InPoint, bool bInDown, unsigned InButton = 0)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
	Event.Type = EEventType::MouseButton;
	Event.Button = InButton;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}

FVec2 FramingPoint(FVec4 InBounds)
{
	return {(InBounds.X + InBounds.Z) / 2, (InBounds.Y + InBounds.W) / 2};
}

void RequireFramingBusy(ISceneViewport& InViewport, const FSceneMutationRequest& InRequest)
{
	const auto Before = InViewport.ViewportState().Camera;
	bool bRejected{};
	try
	{
		InViewport.FrameSelection(InRequest);
	}
	catch (const FSceneEditError& Failure)
	{
		bRejected = Failure.Code == "busy";
	}
	RequireFraming(bRejected && InViewport.ViewportState().Camera == Before,
	               "shared service admitted framing during active interaction");
}

void RequireFramingVisible(const FSceneCameraView& InCamera, FSize InSize, const FBounds& InBounds)
{
	for (const auto Convention : {EDepthConvention::Standard, EDepthConvention::Reversed})
	{
		const auto Matrix = SceneCameraViewProjection(ExtractScenePose(InCamera.World), InCamera.Lens,
		                                              float(InSize.Width) / InSize.Height, Convention);
		for (unsigned Index = 0; Index < 8; ++Index)
		{
			const auto Point = BoundsCorner(InBounds, Index);
			const auto Clip = Transform(Matrix, {Point.X, Point.Y, Point.Z, 1});
			RequireFraming(Clip.W > 0 && std::abs(Clip.X) < Clip.W && std::abs(Clip.Y) < Clip.W && Clip.Z >= 0 &&
			                   Clip.Z <= Clip.W,
			               "framed scene is clipped after selection framing");
		}
	}
}
} // namespace

void FEditorPlugin::PrepareFramingExercise()
{
	for (const auto Handle : Scene->GetNodes(ESceneNodeKind::Model))
	{
		Scene->SetModelVisible(Handle, false);
	}
	FSceneNode Model;
	Model.Model() = FSceneModelComponent{};
	Model.Model()->Asset = PlacementModels.at("Cube").Asset;
	Model.Name = "Framing A";
	Model.Local() = Translation({-3, 0, 0});
	FramingObjects.push_back(Scene->AddNode(Model));
	Model.Name = "Framing B";
	Model.Id.clear();
	Model.Local() = Translation({3, 1, 0});
	FramingObjects.push_back(Scene->AddNode(Model));
	Filter = "Framing";
	bShowLightMarkers = false;
	ResetDocument();
	SelectObject(std::nullopt);
	ViewCamera.World = SceneCameraTransform({0, 0, 20}, {});
	FramingBefore = ViewCamera;
	FramingRevision = Scene->GetRevision();
	FramingSnapshot = Serialize(Scene->Snapshot("FramingAcceptance.hasset"));
	Gui->FocusWindow("Outliner");
}

void FEditorPlugin::CheckFramingResult(FVec3 InCenter)
{
	const auto Pose = ExtractScenePose(ViewCamera.World);
	const auto Before = ExtractScenePose(FramingBefore.World);
	const auto Target = Add(Pose.Eye, ScaleVector(Pose.Forward, ViewCamera.Lens.FocusDistance));
	RequireFraming(Length(Subtract(Target, InCenter)) < .001f, "camera did not focus selection bounds center");
	RequireFraming(Length(Subtract(Pose.Forward, Before.Forward)) < .00001f &&
	                   ViewCamera.Lens.VerticalRadians == FramingBefore.Lens.VerticalRadians,
	               "framing changed orientation or FOV");
	RequireFraming(Scene->GetRevision() == FramingRevision && !IsDirty() && History.empty(),
	               "framing changed document revision, dirty state or history");
	RequireFraming(Serialize(Scene->Snapshot("FramingAcceptance.hasset")) == FramingSnapshot,
	               "framing changed the authored snapshot");
}

void FEditorPlugin::ExerciseFramingSelection(std::vector<FInputEvent>& InEvents)
{
	const auto A = FramingObjects[0];
	const auto B = FramingObjects[1];
	const auto Row = [&](FSceneHandle InHandle)
	{
		return FramingPoint(MultiSelectionRows.at(Scene->FindNode(InHandle)->Id));
	};
	switch (FramingStep)
	{
		case 1:
		case 2:
			FramingClick(InEvents, Row(A), FramingStep == 1);
			break;
		case 3:
			RequireFraming(Selection == A && Gui->IsWindowFocused("Outliner"), "Outliner did not select A");
			FramingKey(InEvents);
			break;
		case 4:
			CheckFramingResult({-3, 0, 0});
			ViewCamera = FramingBefore;
			FramingKey(InEvents, EKey::None, true, false, 1);
			FramingClick(InEvents, Row(B), true);
			break;
		case 5:
			FramingClick(InEvents, Row(B), false);
			break;
		case 6:
			RequireFraming(Selection.All().size() == 2 && Selection == B, "Outliner multi-selection failed");
			FramingKey(InEvents);
			break;
		case 7:
		{
			CheckFramingResult({0, .5f, 0});
			FramingMultiple = ViewCamera;
			FSceneSelection Reversed(B);
			Reversed.Toggle(A);
			SetSelection(std::move(Reversed));
			ViewCamera = FramingBefore;
			Gui->FocusWindow("Details");
			FramingKey(InEvents);
			break;
		}
		case 8:
			RequireFraming(ViewCamera == FramingMultiple, "primary/order or Details focus changed framing");
			CheckFramingResult({0, .5f, 0});
			SelectObject(std::nullopt);
			ViewCamera = FramingBefore;
			Gui->FocusWindow("Viewport");
			break;
		case 9:
		case 10:
		{
			const auto Point = ProjectViewportPoint(ViewCamera, ViewportRegion.Bounds, {-2.75f, .25f, .5f});
			RequireFraming(Point.has_value(), "viewport test surface is off screen");
			FramingClick(InEvents, {Point->X, Point->Y}, FramingStep == 9);
			break;
		}
		case 11:
			RequireFraming(Selection == A && Gui->IsWindowFocused("Viewport"), "viewport did not select A");
			FramingKey(InEvents);
			break;
		case 12:
			CheckFramingResult({-3, 0, 0});
			ViewCamera = FramingBefore;
			Gui->FocusWindow("Content Browser");
			break;
	}
}

void FEditorPlugin::ExerciseFramingGuards(std::vector<FInputEvent>& InEvents)
{
	RequireFraming(ViewCamera == FramingBefore, "guarded shortcut changed camera");
	switch (FramingStep)
	{
		case 13:
			FramingKey(InEvents);
			break;
		case 14:
			Gui->FocusWindow("Outliner");
			break;
		case 15:
			FramingKey(InEvents, EKey::F, true, true);
			break;
		case 16:
			FramingKey(InEvents, EKey::F, true, false, 1);
			break;
		case 17:
		case 18:
			FramingClick(InEvents, FramingPoint(InspectionBounds.at("clipboard/search")), FramingStep == 17);
			break;
		case 19:
		{
			RequireFraming(Gui->IsEditingText(), "search did not own text input");
			FramingKey(InEvents);
			FInputEvent Text;
			Text.Type = EEventType::Text;
			Text.Text = "f";
			InEvents.push_back(Text);
			break;
		}
		case 20:
			RequireFraming(Filter.find('f') != std::string::npos, "F text did not reach search");
			FramingKey(InEvents, EKey::Escape);
			FramingKey(InEvents);
			break;
		case 21:
			Gui->FinishEditing();
			Filter = "Framing";
			ShowOpenScene();
			break;
		case 22:
		{
			bool bRejected{};
			try
			{
				FrameSelection({SceneDocument.Id(), Scene->GetRevision()});
			}
			catch (const FSceneEditError& Failure)
			{
				bRejected = Failure.Code == "busy";
			}
			RequireFraming(bRejected, "shared service admitted framing during a modal operation");
			FramingKey(InEvents);
			break;
		}
	}
}

void FEditorPlugin::ExerciseFramingViewGuards(std::vector<FInputEvent>& InEvents)
{
	RequireFraming(ViewCamera == FramingBefore, "guarded view shortcut changed camera");
	switch (FramingStep)
	{
		case 23:
			bOpenDialog = false;
			Gui->ClosePopups();
			Gui->FocusWindow("Viewport");
			PreviewSceneCamera(Scene->AddNode(MakeSceneCameraNode("framing-preview")));
			FramingRevision = Scene->GetRevision();
			FramingSnapshot = Serialize(Scene->Snapshot("FramingAcceptance.hasset"));
			break;
		case 24:
			FramingKey(InEvents);
			break;
		case 25:
		{
			CheckFramingResult({0, 0, 20 - FramingBefore.Lens.FocusDistance});
			SetPreviewCamera({});
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			InEvents.push_back(Focus);
			FramingKey(InEvents);
			break;
		}
		case 26:
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = true;
			InEvents.push_back(Focus);
			Gui->FocusWindow("Viewport");
			break;
		}
		case 27:
			FramingClick(InEvents, FramingPoint(ViewportRegion.Bounds), true, 1);
			FramingKey(InEvents);
			break;
		case 28:
			RequireFraming(bCameraDragging, "RMB navigation did not start");
			RequireFramingBusy(*this, {SceneDocument.Id(), Scene->GetRevision()});
			RequireFraming(bCameraDragging, "rejected framing interrupted RMB navigation");
			FramingClick(InEvents, FramingPoint(ViewportRegion.Bounds), false, 1);
			SelectObject(std::nullopt);
			break;
		case 29:
			FramingKey(InEvents);
			break;
		case 30:
			SelectObject(FramingObjects[0]);
			FrameSelection({SceneDocument.Id(), Scene->GetRevision()});
			SelectObject(std::nullopt);
			FramingKey(InEvents, EKey::Home);
			break;
	}
}

void FEditorPlugin::ExerciseFramingPopup(std::vector<FInputEvent>& InEvents)
{
	RequireFraming(ViewCamera == FramingBefore, "popup interaction changed camera");
	switch (FramingStep)
	{
		case 32:
		case 33:
			FramingClick(InEvents, FramingPoint(InspectionBounds.at("view/options")), FramingStep == 32);
			break;
		case 34:
			RequireFraming(Gui->HasOpenPopup(), "viewport options popup did not open");
			RequireFraming(!IsDocumentInteractionBusy(), "ordinary popup was masked by document busy state");
			RequireFramingBusy(*this, {SceneDocument.Id(), Scene->GetRevision()});
			FramingKey(InEvents);
			break;
		case 35:
			CheckFramingResult({0, .5f, 0});
			Gui->ClosePopups();
			SelectObject(std::nullopt);
			bFramingVerified = true;
			break;
	}
}

void FEditorPlugin::ExerciseFraming(std::vector<FInputEvent>& InEvents)
{
	if (!Scene->GetStatus().bReady || !bViewportVisible || bFramingVerified)
	{
		return;
	}
	if (++FramingWait % 3 != 0)
	{
		if (FramingWait % 3 == 1)
		{
			FramingKey(InEvents, EKey::F, false);
			FramingKey(InEvents, EKey::Escape, false);
			FramingKey(InEvents, EKey::Home, false);
		}
		return;
	}
	if (!FramingStep)
	{
		PrepareFramingExercise();
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = true;
		InEvents.push_back(Focus);
	}
	else if (FramingStep <= 12)
	{
		ExerciseFramingSelection(InEvents);
	}
	else if (FramingStep <= 22)
	{
		ExerciseFramingGuards(InEvents);
	}
	else if (FramingStep <= 30)
	{
		ExerciseFramingViewGuards(InEvents);
	}
	else if (FramingStep == 31)
	{
		CheckFramingResult({0, .5f, 0});
		RequireFramingVisible(ViewCamera, ViewportSize, {{-3.5f, -.5f, -.5f}, {3.5f, 1.5f, .5f}, true});
		RequireFraming(!Selection, "empty selection was modified");
		FramingBefore = ViewCamera;
		SelectObject(FramingObjects[0]);
		Gui->FocusWindow("Viewport");
	}
	else
	{
		ExerciseFramingPopup(InEvents);
	}
	++FramingStep;
}
} // namespace Hyperion
