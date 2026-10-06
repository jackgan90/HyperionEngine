#include "EditorAcceptanceHarness.h"
#include "Hyperion/Renderer/ViewportRay.h"

namespace Hyperion
{
namespace
{
void CheckPicking(bool bInCondition, const std::string& InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(std::string("Picking acceptance: ") + InMessage);
	}
}

void Pointer(std::vector<FInputEvent>& InEvents, FVec2 InPosition, bool bInDown, unsigned InButton = InputButtons::Left)
{
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = InPosition.X;
	Move.Y = InPosition.Y;
	InEvents.push_back(Move);
	Move.Type = EEventType::MouseButton;
	Move.Button = InButton;
	Move.bDown = bInDown;
	InEvents.push_back(Move);
}
} // namespace

bool FEditorAcceptanceHarness::ExercisePickingScene(std::vector<FInputEvent>& InEvents)
{
	if (Scenario.PickingScene.Is(EPickingSceneState::Complete))
	{
		return true;
	}
	if (Scenario.PickingScene.Is(EPickingSceneState::ExerciseDeletion))
	{
		const auto& Outline = Editor.RenderStats.SelectionOutline;
		CheckPicking(Outline.UnsupportedItems == 0, "native scene material has no outline coverage");
		if (!Outline.Items || Outline.PendingItems)
		{
			return false;
		}
	}
	if ((Scenario.PickingScene.Is(EPickingSceneState::PrepareSurfaceClick) ||
	     Scenario.PickingScene.Is(EPickingSceneState::VerifySurfaceSelection)) &&
	    !ExerciseDeletionInput(InEvents))
	{
		return false;
	}
	const auto Light = Editor.Scene->FindHandle("light-courtyard-3");
	CheckPicking(Light.Scene != 0, "requires Sponza courtyard light 3");
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Surface{Bounds.X + (Bounds.Z - Bounds.X) * .2f, Bounds.Y + (Bounds.W - Bounds.Y) * .7f};
	const FVec2 LightRow{Scenario.PickingLightBounds.X + 60,
	                     (Scenario.PickingLightBounds.Y + Scenario.PickingLightBounds.W) * .5f};
	switch (Scenario.PickingScene.GetState())
	{
		case EPickingSceneState::PressLightRow:
			CheckPicking(Scenario.PickingLightBounds.W > Scenario.PickingLightBounds.Y, "light row is not visible");
			Pointer(InEvents, LightRow, true);
			break;
		case EPickingSceneState::ReleaseLightRow:
			Pointer(InEvents, LightRow, false);
			break;
		case EPickingSceneState::PrepareSurfaceClick:
		{
			auto Candidate = Editor.Rendering;
			Candidate.bReversedZ = !Editor.Options.Rendering.bReversedZ;
			Editor.SetRenderSettings(Editor.RenderSettingsRevision, Candidate);
			CheckPicking(Editor.Selection == Light, "Outliner did not select courtyard light 3");
			CheckPicking(Editor.Gizmo.HitTest(Surface) == ETransformGizmoHandle::None, "surface overlaps light gizmo");
			const auto Ray =
			    MakeViewportRay(Editor.Viewport.ViewCamera, {.2f, .7f}, Editor.Viewport.ViewportSize.Width,
			                    Editor.Viewport.ViewportSize.Height, GetDepthConvention(Editor.Rendering.bReversedZ));
			CheckPicking(Ray.has_value(), "Sponza view ray is unavailable");
			const auto QueryOptions = MakeSceneRayOptions(ESceneRenderPipeline::Deferred);
			const auto Hit = Editor.Scene->Raycast(*Ray, QueryOptions);
			CheckPicking(Hit.Status == ESceneRayStatus::Hit &&
			                 Hit.Handle == Editor.Scene->GetNodes(ESceneNodeKind::Model).front(),
			             "foreground Sponza geometry query failed");
			Pointer(InEvents, Surface, true);
			break;
		}
		case EPickingSceneState::VerifyHeldSurface:
			CheckPicking(Editor.ViewportClick && Editor.Viewport.ViewportRegion.bFocused && !Editor.Gizmo.IsDragging(),
			             "held Sponza click lost image ownership");
			break;
		case EPickingSceneState::ReleaseSurface:
		{
			auto Candidate = Editor.Rendering;
			Candidate.bReversedZ = Editor.Options.Rendering.bReversedZ;
			Editor.SetRenderSettings(Editor.RenderSettingsRevision, Candidate);
			Pointer(InEvents, Surface, false);
			break;
		}
		case EPickingSceneState::VerifySurfaceSelection:
			CheckPicking(Editor.Selection == Editor.Scene->GetNodes(ESceneNodeKind::Model).front(),
			             "held Sponza click did not replace Outliner light selection");
			CheckPicking(Editor.History.empty() && !Editor.IsDirty(), "Sponza selection authored an edit");
			break;
		case EPickingSceneState::ExerciseDeletion:
			ExerciseDeletionHistory();
			CheckPicking(Editor.Selection && Editor.Gizmo.InitialLocal().Values ==
			                                     Editor.Scene->FindNode(*Editor.Selection)->Local().Values,
			             "gizmo retained the light transform after selecting Sponza");
			break;
	}
	switch (Scenario.PickingScene.GetState())
	{
		case EPickingSceneState::PressLightRow:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::AwaitLightPress);
			break;
		case EPickingSceneState::AwaitLightPress:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::ReleaseLightRow);
			break;
		case EPickingSceneState::ReleaseLightRow:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::PrepareSurfaceClick);
			break;
		case EPickingSceneState::PrepareSurfaceClick:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::AwaitSurfacePointer);
			break;
		case EPickingSceneState::AwaitSurfacePointer:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::HoldSurface);
			break;
		case EPickingSceneState::HoldSurface:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::AwaitSurfacePress);
			break;
		case EPickingSceneState::AwaitSurfacePress:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::VerifyHeldSurface);
			break;
		case EPickingSceneState::VerifyHeldSurface:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::ReleaseSurface);
			break;
		case EPickingSceneState::ReleaseSurface:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::VerifySurfaceSelection);
			break;
		case EPickingSceneState::VerifySurfaceSelection:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::ExerciseDeletion);
			break;
		case EPickingSceneState::ExerciseDeletion:
			Scenario.PickingScene.TransitionTo(EPickingSceneState::Complete);
			break;
		default:
			break;
	}
	return false;
}

void FEditorAcceptanceHarness::PreparePickingExercise()
{
	// This synthetic fixture isolates triangle picking; icon priority is exercised by placement acceptance.
	Editor.bShowLightMarkers = false;
	const auto Models = Editor.Scene->GetNodes(ESceneNodeKind::Model);
	CheckPicking(!Models.empty(), "requires a prepared scene");
	const auto Original = Editor.Scene->Find(Models.front())->Data;
	for (const auto Handle : Models)
	{
		Editor.Scene->SetModelVisible(Handle, false);
	}
	std::shared_ptr<const FSceneModelData> Data;
	Editor.Tasks.Wait(Editor.Tasks.Dispatch({EDomain::Worker},
	                                        [&]
	                                        {
		                                        auto Asset = std::make_shared<FModelAsset>();
		                                        Asset->MaterialSlots = {Original->Asset->MaterialSlots.front()};
		                                        FModelPrimitive Primitive;
		                                        Primitive.Positions = {-2, -2, 0, 2, -2, 0, 0, 2, 0};
		                                        Primitive.Normals = {0, 0, 1, 0, 0, 1, 0, 0, 1};
		                                        Primitive.Indices = {0, 1, 2};
		                                        Primitive.Material = 0;
		                                        Asset->Primitives = {Primitive};
		                                        FModelNode Node;
		                                        Node.Primitives = {0};
		                                        Asset->Nodes = {Node};
		                                        Asset->Roots = {0};
		                                        auto Prepared = std::make_shared<FSceneModelData>(
		                                            *PrepareSceneModel(Asset, {Original->Materials.front()}));
		                                        Prepared->MaterialSnapshots = {Original->MaterialSnapshots.front()};
		                                        Prepared->QueryGeometry = PrepareSceneModelGeometry(*Asset);
		                                        Data = std::move(Prepared);
	                                        }));
	FSceneModel Model;
	Model.Name = "Picking far";
	Model.Data = Data;
	Scenario.PickingFar = Editor.Scene->Add(Model);
	Model.Name = "Picking near";
	Model.World = Translation({0, 0, 1});
	Scenario.PickingNear = Editor.Scene->Add(Model);
	Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
	Scenario.PickingPreview = Editor.Scene->AddNode(MakeSceneCameraNode("picking-preview", {0, 0, 10}, {}));
	Editor.ResetDocument();
	Editor.SelectObject(std::nullopt);
}

void FEditorAcceptanceHarness::ExercisePickingSelection(std::vector<FInputEvent>& InEvents, FVec2 InCenter,
                                                        FVec2 InEmpty)
{
	switch (Scenario.Picking.GetState())
	{
		case EPickingState::PressModel:
			Pointer(InEvents, InCenter, true);
			Scenario.Picking.TransitionTo(EPickingState::ReleaseModel);
			break;
		case EPickingState::ReleaseModel:
			Pointer(InEvents, InCenter, false);
			Scenario.Picking.TransitionTo(EPickingState::VerifyNearestModel);
			break;
		case EPickingState::VerifyNearestModel:
			CheckPicking(
			    Editor.Selection == Scenario.PickingNear,
			    "nearest triangle was not selected; selected " +
			        (Editor.Selection ? Editor.Scene->FindNode(*Editor.Selection)->Id : std::string("nothing")));
			CheckPicking(Editor.History.empty() && !Editor.IsDirty(), "selection changed document history");
			Scenario.Picking.TransitionTo(EPickingState::PressEmptySpace);
			break;
		case EPickingState::PressEmptySpace:
			Pointer(InEvents, InEmpty, true);
			Scenario.Picking.TransitionTo(EPickingState::ReleaseEmptySpace);
			break;
		case EPickingState::ReleaseEmptySpace:
			Pointer(InEvents, InEmpty, false);
			Scenario.Picking.TransitionTo(EPickingState::VerifyEmptySelection);
			break;
		case EPickingState::VerifyEmptySelection:
			CheckPicking(!Editor.Selection, "empty click did not clear selection");
			Scenario.Picking.TransitionTo(EPickingState::VerifyClearedSelectionAndPressModel);
			break;
		case EPickingState::VerifyClearedSelectionAndPressModel:
			CheckPicking(!Editor.Selection, "outliner restored cleared selection");
			Pointer(InEvents, InCenter, true);
			Scenario.Picking.TransitionTo(EPickingState::DragModelPointer);
			break;
		case EPickingState::DragModelPointer:
		case EPickingState::ReleaseDraggedPointer:
			Pointer(InEvents, {InCenter.X + 50, InCenter.Y}, Scenario.Picking.Is(EPickingState::DragModelPointer));
			Scenario.Picking.TransitionTo(Scenario.Picking.Is(EPickingState::DragModelPointer)
			                                  ? EPickingState::ReleaseDraggedPointer
			                                  : EPickingState::VerifyDragAndPressModel);
			break;
		case EPickingState::VerifyDragAndPressModel:
			CheckPicking(!Editor.Selection, "drag selected a model");
			Pointer(InEvents, InCenter, true);
			Scenario.Picking.TransitionTo(EPickingState::BeginNavigation);
			break;
		case EPickingState::BeginNavigation:
			Pointer(InEvents, InCenter, true, 1);
			Scenario.Picking.TransitionTo(EPickingState::ReleaseModelDuringNavigation);
			break;
		case EPickingState::ReleaseModelDuringNavigation:
			Pointer(InEvents, InCenter, false);
			Scenario.Picking.TransitionTo(EPickingState::EndNavigation);
			break;
		case EPickingState::EndNavigation:
			Pointer(InEvents, InCenter, false, 1);
			Scenario.Picking.TransitionTo(EPickingState::VerifyNavigationAndPressModel);
			break;
		case EPickingState::VerifyNavigationAndPressModel:
			CheckPicking(!Editor.Selection, "navigation selected a model");
			Pointer(InEvents, InCenter, true);
			Scenario.Picking.TransitionTo(EPickingState::LoseFocus);
			break;
		case EPickingState::LoseFocus:
		case EPickingState::RestoreFocusAndRelease:
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = Scenario.Picking.Is(EPickingState::RestoreFocusAndRelease);
			InEvents.push_back(Focus);
			if (Focus.bDown)
			{
				Pointer(InEvents, InCenter, false);
			}
			Scenario.Picking.TransitionTo(Scenario.Picking.Is(EPickingState::LoseFocus)
			                                  ? EPickingState::RestoreFocusAndRelease
			                                  : EPickingState::VerifyFocusLoss);
			break;
		}
		case EPickingState::VerifyFocusLoss:
			CheckPicking(!Editor.Selection, "focus loss completed a click");
			Scenario.Picking.TransitionTo(EPickingState::PressBeforeViewChange);
			break;
		case EPickingState::PressBeforeViewChange:
			Pointer(InEvents, InCenter, true);
			Scenario.Picking.TransitionTo(EPickingState::ChangeView);
			break;
		case EPickingState::ChangeView:
			Editor.Viewport.ViewCamera.World = Translation({1, 0, 10});
			Scenario.Picking.TransitionTo(EPickingState::ReleaseAfterViewChange);
			break;
		case EPickingState::ReleaseAfterViewChange:
			Pointer(InEvents, InCenter, false);
			Scenario.Picking.TransitionTo(EPickingState::VerifyViewChange);
			break;
		case EPickingState::VerifyViewChange:
			CheckPicking(!Editor.Selection, "view change completed a click");
			Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
			Scenario.Picking.TransitionTo(EPickingState::SetPreviewCamera);
			break;

		case EPickingState::AwaitPickingLayout:
			Scenario.Picking.TransitionTo(EPickingState::PressModel);
			break;
	}
}

void FEditorAcceptanceHarness::ExercisePickingView(std::vector<FInputEvent>& InEvents, FVec2 InCenter)
{
	switch (Scenario.Picking.GetState())
	{
		case EPickingState::SetPreviewCamera:
			Editor.SetPreviewCamera(Scenario.PickingPreview);
			Scenario.Picking.TransitionTo(EPickingState::PressPreviewModel);
			break;
		case EPickingState::PressPreviewModel:
			Pointer(InEvents, InCenter, true);
			Scenario.Picking.TransitionTo(EPickingState::ReleasePreviewModel);
			break;
		case EPickingState::ReleasePreviewModel:
			Pointer(InEvents, InCenter, false);
			Scenario.Picking.TransitionTo(EPickingState::VerifyPreviewSelection);
			break;
		case EPickingState::VerifyPreviewSelection:
			CheckPicking(Editor.Selection == Scenario.PickingNear, "preview camera picking failed");
			Scenario.Picking.TransitionTo(EPickingState::DisablePreviewCamera);
			break;
		case EPickingState::DisablePreviewCamera:
			Editor.Scene->SetEnabled(Scenario.PickingPreview, false);
			Editor.SelectObject(Scenario.PickingFar);
			Scenario.Picking.TransitionTo(EPickingState::PressInvalidPreview);
			break;
		case EPickingState::PressInvalidPreview:
			Pointer(InEvents, InCenter, true);
			Scenario.Picking.TransitionTo(EPickingState::ReleaseInvalidPreview);
			break;
		case EPickingState::ReleaseInvalidPreview:
			Pointer(InEvents, InCenter, false);
			Scenario.Picking.TransitionTo(EPickingState::VerifyInvalidPreview);
			break;
		case EPickingState::VerifyInvalidPreview:
			CheckPicking(Editor.Selection == Scenario.PickingFar,
			             "invalid preview cleared selection or used editor camera");
			Editor.Scene->SetEnabled(Scenario.PickingPreview, true);
			Editor.SelectObject(std::nullopt);
			Scenario.Picking.TransitionTo(EPickingState::PressBeforeResize);
			break;
		case EPickingState::PressBeforeResize:
			Pointer(InEvents, InCenter, true);
			Scenario.Picking.TransitionTo(EPickingState::ResizeWindow);
			break;
		case EPickingState::ResizeWindow:
			Editor.Window->Resize({1300, 800});
			Scenario.Picking.TransitionTo(EPickingState::AwaitResize);
			break;
		case EPickingState::ReleaseAfterResize:
			Pointer(InEvents, InCenter, false);
			Scenario.Picking.TransitionTo(EPickingState::VerifyResizeAndPrepareGizmo);
			break;
		case EPickingState::VerifyResizeAndPrepareGizmo:
			CheckPicking(!Editor.Selection, "resize completed a stale click");
			Editor.SetPreviewCamera(std::nullopt);
			Editor.SelectObject(Scenario.PickingFar);
			Scenario.Picking.TransitionTo(EPickingState::PressGizmo);
			break;
		case EPickingState::PressGizmo:
			Pointer(InEvents, InCenter, true);
			Scenario.Picking.TransitionTo(EPickingState::VerifyGizmoOwnership);
			break;
		case EPickingState::VerifyGizmoOwnership:
			CheckPicking(Editor.Gizmo.IsDragging(), "gizmo did not own center press");
			Scenario.Picking.TransitionTo(EPickingState::ReleaseGizmo);
			break;
		case EPickingState::ReleaseGizmo:
			Pointer(InEvents, InCenter, false);
			Scenario.Picking.TransitionTo(EPickingState::VerifyGizmoAndReopen);
			break;
		case EPickingState::VerifyGizmoAndReopen:
			CheckPicking(Editor.Selection == Scenario.PickingFar, "picking stole a gizmo gesture");
			CheckPicking(Editor.History.empty() && !Editor.IsDirty(), "interaction authored unintended edits");
			Editor.bShowLightMarkers = true;
			Editor.OpenScene(Editor.CurrentPath);
			Scenario.Picking.TransitionTo(EPickingState::VerifyReopen);
			break;
		case EPickingState::VerifyReopen:
			CheckPicking(!Editor.Scene->Find(Scenario.PickingNear) && !Editor.ViewportClick,
			             "reopen retained stale picking state");
			Scenario.bPickingVerified = true;
			Scenario.Picking.TransitionTo(EPickingState::Complete);
			break;

		case EPickingState::AwaitResize:
			Scenario.Picking.TransitionTo(EPickingState::ReleaseAfterResize);
			break;
	}
}

void FEditorAcceptanceHarness::ExercisePickingInput(std::vector<FInputEvent>& InEvents)
{
	if (Editor.ReadyFrames < 10 || !Editor.Viewport.bViewportVisible)
	{
		return;
	}
	if (!ExercisePickingScene(InEvents))
	{
		return;
	}
	if (Scenario.Picking.Is(EPickingState::PreparePicking))
	{
		PreparePickingExercise();
		Scenario.Picking.TransitionTo(EPickingState::AwaitPickingLayout);
		return;
	}
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	const FVec2 Empty{Bounds.X + 10, Bounds.Y + 10};
	switch (Scenario.Picking.GetState())
	{
		case EPickingState::AwaitPickingLayout:
		case EPickingState::PressModel:
		case EPickingState::ReleaseModel:
		case EPickingState::VerifyNearestModel:
		case EPickingState::PressEmptySpace:
		case EPickingState::ReleaseEmptySpace:
		case EPickingState::VerifyEmptySelection:
		case EPickingState::VerifyClearedSelectionAndPressModel:
		case EPickingState::DragModelPointer:
		case EPickingState::ReleaseDraggedPointer:
		case EPickingState::VerifyDragAndPressModel:
		case EPickingState::BeginNavigation:
		case EPickingState::ReleaseModelDuringNavigation:
		case EPickingState::EndNavigation:
		case EPickingState::VerifyNavigationAndPressModel:
		case EPickingState::LoseFocus:
		case EPickingState::RestoreFocusAndRelease:
		case EPickingState::VerifyFocusLoss:
		case EPickingState::PressBeforeViewChange:
		case EPickingState::ChangeView:
		case EPickingState::ReleaseAfterViewChange:
		case EPickingState::VerifyViewChange:
			ExercisePickingSelection(InEvents, Center, Empty);
			break;
		case EPickingState::SetPreviewCamera:
		case EPickingState::PressPreviewModel:
		case EPickingState::ReleasePreviewModel:
		case EPickingState::VerifyPreviewSelection:
		case EPickingState::DisablePreviewCamera:
		case EPickingState::PressInvalidPreview:
		case EPickingState::ReleaseInvalidPreview:
		case EPickingState::VerifyInvalidPreview:
		case EPickingState::PressBeforeResize:
		case EPickingState::ResizeWindow:
		case EPickingState::AwaitResize:
		case EPickingState::ReleaseAfterResize:
		case EPickingState::VerifyResizeAndPrepareGizmo:
		case EPickingState::PressGizmo:
		case EPickingState::VerifyGizmoOwnership:
		case EPickingState::ReleaseGizmo:
		case EPickingState::VerifyGizmoAndReopen:
		case EPickingState::VerifyReopen:
			ExercisePickingView(InEvents, Center);
			break;
		default:
			break;
	}
}
} // namespace Hyperion
