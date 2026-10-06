#include "EditorAcceptanceHarness.h"
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

void FramingClick(std::vector<FInputEvent>& InEvents, FVec2 InPoint, bool bInDown,
                  unsigned InButton = InputButtons::Left)
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
		bRejected = Failure.Code == SceneEditErrors::Busy;
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

void RequireFramingNavigation(FSceneCameraView& InCamera, FSize InSize)
{
	const auto Before = InCamera;
	FSceneCameraController Navigation(ESceneCameraNavigationMode::Fly);
	Navigation.SetMovementSpeed(100);
	std::vector<FInputEvent> Events;
	FramingClick(Events, {}, true, 1);
	FramingKey(Events, EKey::S);
	Navigation.Input(InCamera, Events, false, false);
	Navigation.Advance(InCamera, .1f);
	const auto BeforePose = ExtractScenePose(Before.World);
	const auto AfterPose = ExtractScenePose(InCamera.World);
	RequireFraming(Dot(Subtract(BeforePose.Eye, AfterPose.Eye), BeforePose.Forward) > 9.99f,
	               "framed camera did not move backward");
	RequireFraming(InCamera.Lens == Before.Lens, "backward navigation changed the framed camera lens");
	RequireFramingVisible(InCamera, InSize, {{-3.5f, -.5f, -.5f}, {3.5f, 1.5f, .5f}, true});
}
} // namespace

void FEditorAcceptanceHarness::PrepareFramingExercise()
{
	for (const auto Handle : Editor.Scene->GetNodes(ESceneNodeKind::Model))
	{
		Editor.Scene->SetModelVisible(Handle, false);
	}
	FSceneNode Model;
	Model.Model() = FSceneModelComponent{};
	Model.Model()->Asset = Editor.PlacementModels.at("Cube").Asset;
	Model.Name = "Framing A";
	Model.Local() = Translation({-3, 0, 0});
	Scenario.FramingObjects.push_back(Editor.Scene->AddNode(Model));
	Model.Name = "Framing B";
	Model.Id.clear();
	Model.Local() = Translation({3, 1, 0});
	Scenario.FramingObjects.push_back(Editor.Scene->AddNode(Model));
	Editor.Filter = "Framing";
	Editor.bShowLightMarkers = false;
	Editor.ResetDocument();
	Editor.SelectObject(std::nullopt);
	Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 20}, {});
	Scenario.FramingBefore = Editor.Viewport.ViewCamera;
	Scenario.FramingRevision = Editor.Scene->GetRevision();
	Scenario.FramingSnapshot = Serialize(Editor.Scene->Snapshot("FramingAcceptance.hasset"));
	Editor.Gui->FocusWindow("Outliner");
}

void FEditorAcceptanceHarness::CheckFramingResult(FVec3 InCenter)
{
	const auto Pose = ExtractScenePose(Editor.Viewport.ViewCamera.World);
	const auto Before = ExtractScenePose(Scenario.FramingBefore.World);
	const auto Target = Add(Pose.Eye, ScaleVector(Pose.Forward, Editor.Viewport.ViewCamera.Lens.FocusDistance));
	RequireFraming(Length(Subtract(Target, InCenter)) < .001f, "camera did not focus selection bounds center");
	RequireFraming(Length(Subtract(Pose.Forward, Before.Forward)) < .00001f &&
	                   Editor.Viewport.ViewCamera.Lens.VerticalRadians == Scenario.FramingBefore.Lens.VerticalRadians,
	               "framing changed orientation or FOV");
	RequireFraming(Editor.Viewport.ViewCamera.Lens.Far >= Scenario.FramingBefore.Lens.Far,
	               "framing shortened the browsing far plane");
	RequireFraming(Editor.Scene->GetRevision() == Scenario.FramingRevision && !Editor.IsDirty() &&
	                   Editor.History.empty(),
	               "framing changed document revision, dirty state or history");
	RequireFraming(Serialize(Editor.Scene->Snapshot("FramingAcceptance.hasset")) == Scenario.FramingSnapshot,
	               "framing changed the authored snapshot");
}

void FEditorAcceptanceHarness::ExerciseFramingSelection(std::vector<FInputEvent>& InEvents)
{
	const auto A = Scenario.FramingObjects[0];
	const auto B = Scenario.FramingObjects[1];
	const auto Row = [&](FSceneHandle InHandle)
	{
		return FramingPoint(Scenario.MultiSelectionRows.at(Editor.Scene->FindNode(InHandle)->Id));
	};
	switch (Scenario.Framing.Progress.GetState())
	{
		case EFramingState::PressOutlinerObject:
		case EFramingState::ReleaseOutlinerObject:
			FramingClick(InEvents, Row(A), Scenario.Framing.Progress.Is(EFramingState::PressOutlinerObject));
			Scenario.Framing.Progress.TransitionTo(Scenario.Framing.Progress.Is(EFramingState::PressOutlinerObject)
			                                           ? EFramingState::ReleaseOutlinerObject
			                                           : EFramingState::FrameOutlinerSelection);
			break;
		case EFramingState::FrameOutlinerSelection:
			RequireFraming(Editor.Selection == A && Editor.Gui->IsWindowFocused("Outliner"),
			               "Outliner did not select A");
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifySingleAndPressSecondObject);
			break;
		case EFramingState::VerifySingleAndPressSecondObject:
			CheckFramingResult({-3, 0, 0});
			Editor.Viewport.ViewCamera = Scenario.FramingBefore;
			FramingKey(InEvents, EKey::None, true, false, 1);
			FramingClick(InEvents, Row(B), true);
			Scenario.Framing.Progress.TransitionTo(EFramingState::ReleaseSecondObject);
			break;
		case EFramingState::ReleaseSecondObject:
			FramingClick(InEvents, Row(B), false);
			Scenario.Framing.Progress.TransitionTo(EFramingState::FrameMultipleSelection);
			break;
		case EFramingState::FrameMultipleSelection:
			RequireFraming(Editor.Selection.All().size() == 2 && Editor.Selection == B,
			               "Outliner multi-selection failed");
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifyMultipleAndReversePrimary);
			break;
		case EFramingState::VerifyMultipleAndReversePrimary:
		{
			CheckFramingResult({0, .5f, 0});
			Scenario.FramingMultiple = Editor.Viewport.ViewCamera;
			FSceneSelection Reversed(B);
			Reversed.Toggle(A);
			Editor.SetSelection(std::move(Reversed));
			Editor.Viewport.ViewCamera = Scenario.FramingBefore;
			Editor.Gui->FocusWindow("Details");
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifyDetailsAndClearSelection);
			break;
		}
		case EFramingState::VerifyDetailsAndClearSelection:
			RequireFraming(Editor.Viewport.ViewCamera == Scenario.FramingMultiple,
			               "primary/order or Details focus changed framing");
			CheckFramingResult({0, .5f, 0});
			Editor.SelectObject(std::nullopt);
			Editor.Viewport.ViewCamera = Scenario.FramingBefore;
			Editor.Gui->FocusWindow("Viewport");
			Scenario.Framing.Progress.TransitionTo(EFramingState::PressViewportObject);
			break;
		case EFramingState::PressViewportObject:
		case EFramingState::ReleaseViewportObject:
		{
			const auto Point = ProjectViewportPoint(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportRegion.Bounds,
			                                        {-2.75f, .25f, .5f});
			RequireFraming(Point.has_value(), "viewport test surface is off screen");
			FramingClick(InEvents, {Point->X, Point->Y},
			             Scenario.Framing.Progress.Is(EFramingState::PressViewportObject));
			Scenario.Framing.Progress.TransitionTo(Scenario.Framing.Progress.Is(EFramingState::PressViewportObject)
			                                           ? EFramingState::ReleaseViewportObject
			                                           : EFramingState::FrameViewportSelection);
			break;
		}
		case EFramingState::FrameViewportSelection:
			RequireFraming(Editor.Selection == A && Editor.Gui->IsWindowFocused("Viewport"),
			               "viewport did not select A");
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifyViewportAndFocusContent);
			break;
		case EFramingState::VerifyViewportAndFocusContent:
			CheckFramingResult({-3, 0, 0});
			RequireFramingNavigation(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportSize);
			Editor.Viewport.ViewCamera = Scenario.FramingBefore;
			Editor.Gui->FocusWindow("Content Browser");
			Scenario.Framing.Progress.TransitionTo(EFramingState::FrameInContentBrowser);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseFramingGuards(std::vector<FInputEvent>& InEvents)
{
	RequireFraming(Editor.Viewport.ViewCamera == Scenario.FramingBefore, "guarded shortcut changed camera");
	switch (Scenario.Framing.Progress.GetState())
	{
		case EFramingState::FrameInContentBrowser:
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::FocusOutliner);
			break;
		case EFramingState::FocusOutliner:
			Editor.Gui->FocusWindow("Outliner");
			Scenario.Framing.Progress.TransitionTo(EFramingState::RepeatFrameShortcut);
			break;
		case EFramingState::RepeatFrameShortcut:
			FramingKey(InEvents, EKey::F, true, true);
			Scenario.Framing.Progress.TransitionTo(EFramingState::ModifiedFrameShortcut);
			break;
		case EFramingState::ModifiedFrameShortcut:
			FramingKey(InEvents, EKey::F, true, false, 1);
			Scenario.Framing.Progress.TransitionTo(EFramingState::PressSearch);
			break;
		case EFramingState::PressSearch:
		case EFramingState::ReleaseSearch:
			FramingClick(InEvents, FramingPoint(Scenario.InspectionBounds.at("clipboard/search")),
			             Scenario.Framing.Progress.Is(EFramingState::PressSearch));
			Scenario.Framing.Progress.TransitionTo(Scenario.Framing.Progress.Is(EFramingState::PressSearch)
			                                           ? EFramingState::ReleaseSearch
			                                           : EFramingState::FrameDuringTextEdit);
			break;
		case EFramingState::FrameDuringTextEdit:
		{
			RequireFraming(Editor.Gui->IsEditingText(), "search did not own text input");
			FramingKey(InEvents);
			FInputEvent Text;
			Text.Type = EEventType::Text;
			Text.Text = "f";
			InEvents.push_back(Text);
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifyTextAndEscape);
			break;
		}
		case EFramingState::VerifyTextAndEscape:
			RequireFraming(Editor.Filter.find('f') != std::string::npos, "F text did not reach search");
			FramingKey(InEvents, EKey::Escape);
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::OpenSceneDialog);
			break;
		case EFramingState::OpenSceneDialog:
			Editor.Gui->FinishEditing();
			Editor.Filter = "Framing";
			Editor.ShowOpenScene();
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifyModalGuard);
			break;
		case EFramingState::VerifyModalGuard:
		{
			bool bRejected{};
			try
			{
				Editor.FrameSelection({Editor.SceneDocument.Id(), Editor.Scene->GetRevision()});
			}
			catch (const FSceneEditError& Failure)
			{
				bRejected = Failure.Code == SceneEditErrors::Busy;
			}
			RequireFraming(bRejected, "shared service admitted framing during a modal operation");
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::PreparePreviewGuard);
			break;
		}
	}
}

void FEditorAcceptanceHarness::ExerciseFramingViewGuards(std::vector<FInputEvent>& InEvents)
{
	RequireFraming(Editor.Viewport.ViewCamera == Scenario.FramingBefore, "guarded view shortcut changed camera");
	switch (Scenario.Framing.Progress.GetState())
	{
		case EFramingState::PreparePreviewGuard:
			Editor.bOpenDialog = false;
			Editor.Gui->ClosePopups();
			Editor.Gui->FocusWindow("Viewport");
			Editor.PreviewSceneCamera(Editor.Scene->AddNode(MakeSceneCameraNode("framing-preview")));
			Scenario.FramingRevision = Editor.Scene->GetRevision();
			Scenario.FramingSnapshot = Serialize(Editor.Scene->Snapshot("FramingAcceptance.hasset"));
			Scenario.Framing.Progress.TransitionTo(EFramingState::FramePreview);
			break;
		case EFramingState::FramePreview:
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifyPreviewAndLoseFocus);
			break;
		case EFramingState::VerifyPreviewAndLoseFocus:
		{
			CheckFramingResult({0, 0, 20 - Scenario.FramingBefore.Lens.FocusDistance});
			Editor.SetPreviewCamera({});
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			InEvents.push_back(Focus);
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::RestoreFocus);
			break;
		}
		case EFramingState::RestoreFocus:
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = true;
			InEvents.push_back(Focus);
			Editor.Gui->FocusWindow("Viewport");
			Scenario.Framing.Progress.TransitionTo(EFramingState::BeginNavigationAndFrame);
			break;
		}
		case EFramingState::BeginNavigationAndFrame:
			FramingClick(InEvents, FramingPoint(Editor.Viewport.ViewportRegion.Bounds), true, 1);
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifyNavigationAndRelease);
			break;
		case EFramingState::VerifyNavigationAndRelease:
			RequireFraming(Editor.Viewport.bCameraDragging, "RMB navigation did not start");
			RequireFramingBusy(Editor, {Editor.SceneDocument.Id(), Editor.Scene->GetRevision()});
			RequireFraming(Editor.Viewport.bCameraDragging, "rejected framing interrupted RMB navigation");
			FramingClick(InEvents, FramingPoint(Editor.Viewport.ViewportRegion.Bounds), false, 1);
			Editor.SelectObject(std::nullopt);
			Scenario.Framing.Progress.TransitionTo(EFramingState::FrameEmptySelection);
			break;
		case EFramingState::FrameEmptySelection:
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::RestoreInitialView);
			break;
		case EFramingState::RestoreInitialView:
			Editor.SelectObject(Scenario.FramingObjects[0]);
			Editor.FrameSelection({Editor.SceneDocument.Id(), Editor.Scene->GetRevision()});
			Editor.SelectObject(std::nullopt);
			FramingKey(InEvents, EKey::Home);
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifyInitialViewAndSelectMultiple);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseFramingPopup(std::vector<FInputEvent>& InEvents)
{
	RequireFraming(Editor.Viewport.ViewCamera == Scenario.FramingBefore, "popup interaction changed camera");
	switch (Scenario.Framing.Progress.GetState())
	{
		case EFramingState::PressViewOptions:
		case EFramingState::ReleaseViewOptions:
			FramingClick(InEvents, FramingPoint(Scenario.InspectionBounds.at("view/options")),
			             Scenario.Framing.Progress.Is(EFramingState::PressViewOptions));
			Scenario.Framing.Progress.TransitionTo(Scenario.Framing.Progress.Is(EFramingState::PressViewOptions)
			                                           ? EFramingState::ReleaseViewOptions
			                                           : EFramingState::VerifyPopupGuard);
			break;
		case EFramingState::VerifyPopupGuard:
			RequireFraming(Editor.Gui->HasOpenPopup(), "viewport options popup did not open");
			RequireFraming(!Editor.IsDocumentInteractionBusy(), "ordinary popup was masked by document busy state");
			RequireFramingBusy(Editor, {Editor.SceneDocument.Id(), Editor.Scene->GetRevision()});
			FramingKey(InEvents);
			Scenario.Framing.Progress.TransitionTo(EFramingState::VerifyPopupFraming);
			break;
		case EFramingState::VerifyPopupFraming:
			CheckFramingResult({0, .5f, 0});
			Editor.Gui->ClosePopups();
			Editor.SelectObject(std::nullopt);
			Scenario.bFramingVerified = true;
			Scenario.Framing.Progress.TransitionTo(EFramingState::Complete);
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseFraming(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Scene->GetStatus().bReady || !Editor.Viewport.bViewportVisible || Scenario.bFramingVerified)
	{
		return;
	}
	const auto InputPhase = Scenario.Framing.Cadence.Advance();
	if (InputPhase != EAcceptanceCadencePhase::Execute)
	{
		if (InputPhase == EAcceptanceCadencePhase::ReleaseKeys)
		{
			FramingKey(InEvents, EKey::F, false);
			FramingKey(InEvents, EKey::Escape, false);
			FramingKey(InEvents, EKey::Home, false);
		}
		return;
	}
	if (Scenario.Framing.Progress.Is(EFramingState::PrepareFraming))
	{
		PrepareFramingExercise();
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = true;
		InEvents.push_back(Focus);
		Scenario.Framing.Progress.TransitionTo(EFramingState::PressOutlinerObject);
	}
	else if (Scenario.Framing.Progress.IsAny(
	             {EFramingState::PressOutlinerObject, EFramingState::ReleaseOutlinerObject,
	              EFramingState::FrameOutlinerSelection, EFramingState::VerifySingleAndPressSecondObject,
	              EFramingState::ReleaseSecondObject, EFramingState::FrameMultipleSelection,
	              EFramingState::VerifyMultipleAndReversePrimary, EFramingState::VerifyDetailsAndClearSelection,
	              EFramingState::PressViewportObject, EFramingState::ReleaseViewportObject,
	              EFramingState::FrameViewportSelection, EFramingState::VerifyViewportAndFocusContent}))
	{
		ExerciseFramingSelection(InEvents);
	}
	else if (Scenario.Framing.Progress.IsAny({EFramingState::FrameInContentBrowser, EFramingState::FocusOutliner,
	                                          EFramingState::RepeatFrameShortcut, EFramingState::ModifiedFrameShortcut,
	                                          EFramingState::PressSearch, EFramingState::ReleaseSearch,
	                                          EFramingState::FrameDuringTextEdit, EFramingState::VerifyTextAndEscape,
	                                          EFramingState::OpenSceneDialog, EFramingState::VerifyModalGuard}))
	{
		ExerciseFramingGuards(InEvents);
	}
	else if (Scenario.Framing.Progress.IsAny({EFramingState::PreparePreviewGuard, EFramingState::FramePreview,
	                                          EFramingState::VerifyPreviewAndLoseFocus, EFramingState::RestoreFocus,
	                                          EFramingState::BeginNavigationAndFrame,
	                                          EFramingState::VerifyNavigationAndRelease,
	                                          EFramingState::FrameEmptySelection, EFramingState::RestoreInitialView}))
	{
		ExerciseFramingViewGuards(InEvents);
	}
	else if (Scenario.Framing.Progress.Is(EFramingState::VerifyInitialViewAndSelectMultiple))
	{
		CheckFramingResult({0, .5f, 0});
		RequireFramingVisible(Editor.Viewport.ViewCamera, Editor.Viewport.ViewportSize,
		                      {{-3.5f, -.5f, -.5f}, {3.5f, 1.5f, .5f}, true});
		RequireFraming(!Editor.Selection, "empty selection was modified");
		Scenario.FramingBefore = Editor.Viewport.ViewCamera;
		Editor.SelectObject(Scenario.FramingObjects[0]);
		Editor.Gui->FocusWindow("Viewport");
		Scenario.Framing.Progress.TransitionTo(EFramingState::PressViewOptions);
	}
	else
	{
		ExerciseFramingPopup(InEvents);
	}
}
} // namespace Hyperion
