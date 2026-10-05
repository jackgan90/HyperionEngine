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
	if (Scenario.PickingSceneStep > 10)
	{
		return true;
	}
	if (Scenario.PickingSceneStep == 10)
	{
		const auto& Outline = Editor.RenderStats.SelectionOutline;
		CheckPicking(Outline.UnsupportedItems == 0, "native scene material has no outline coverage");
		if (!Outline.Items || Outline.PendingItems)
		{
			return false;
		}
	}
	if ((Scenario.PickingSceneStep == 3 || Scenario.PickingSceneStep == 9) && !ExerciseDeletionInput(InEvents))
	{
		return false;
	}
	const auto Light = Editor.Scene->FindHandle("light-courtyard-3");
	CheckPicking(Light.Scene != 0, "requires Sponza courtyard light 3");
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Surface{Bounds.X + (Bounds.Z - Bounds.X) * .2f, Bounds.Y + (Bounds.W - Bounds.Y) * .7f};
	const FVec2 LightRow{Scenario.PickingLightBounds.X + 60,
	                     (Scenario.PickingLightBounds.Y + Scenario.PickingLightBounds.W) * .5f};
	switch (Scenario.PickingSceneStep)
	{
		case 0:
			CheckPicking(Scenario.PickingLightBounds.W > Scenario.PickingLightBounds.Y, "light row is not visible");
			Pointer(InEvents, LightRow, true);
			break;
		case 2:
			Pointer(InEvents, LightRow, false);
			break;
		case 3:
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
		case 7:
			CheckPicking(Editor.ViewportClick && Editor.Viewport.ViewportRegion.bFocused && !Editor.Gizmo.IsDragging(),
			             "held Sponza click lost image ownership");
			break;
		case 8:
		{
			auto Candidate = Editor.Rendering;
			Candidate.bReversedZ = Editor.Options.Rendering.bReversedZ;
			Editor.SetRenderSettings(Editor.RenderSettingsRevision, Candidate);
			Pointer(InEvents, Surface, false);
			break;
		}
		case 9:
			CheckPicking(Editor.Selection == Editor.Scene->GetNodes(ESceneNodeKind::Model).front(),
			             "held Sponza click did not replace Outliner light selection");
			CheckPicking(Editor.History.empty() && !Editor.IsDirty(), "Sponza selection authored an edit");
			break;
		case 10:
			ExerciseDeletionHistory();
			CheckPicking(Editor.Selection && Editor.Gizmo.InitialLocal().Values ==
			                                     Editor.Scene->FindNode(*Editor.Selection)->Local().Values,
			             "gizmo retained the light transform after selecting Sponza");
			break;
	}
	++Scenario.PickingSceneStep;
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
	switch (Scenario.PickingExerciseStep)
	{
		case 2:
			Pointer(InEvents, InCenter, true);
			break;
		case 3:
			Pointer(InEvents, InCenter, false);
			break;
		case 4:
			CheckPicking(
			    Editor.Selection == Scenario.PickingNear,
			    "nearest triangle was not selected; selected " +
			        (Editor.Selection ? Editor.Scene->FindNode(*Editor.Selection)->Id : std::string("nothing")));
			CheckPicking(Editor.History.empty() && !Editor.IsDirty(), "selection changed document history");
			break;
		case 5:
			Pointer(InEvents, InEmpty, true);
			break;
		case 6:
			Pointer(InEvents, InEmpty, false);
			break;
		case 7:
			CheckPicking(!Editor.Selection, "empty click did not clear selection");
			break;
		case 8:
			CheckPicking(!Editor.Selection, "outliner restored cleared selection");
			Pointer(InEvents, InCenter, true);
			break;
		case 9:
		case 10:
			Pointer(InEvents, {InCenter.X + 50, InCenter.Y}, Scenario.PickingExerciseStep == 9);
			break;
		case 11:
			CheckPicking(!Editor.Selection, "drag selected a model");
			Pointer(InEvents, InCenter, true);
			break;
		case 12:
			Pointer(InEvents, InCenter, true, 1);
			break;
		case 13:
			Pointer(InEvents, InCenter, false);
			break;
		case 14:
			Pointer(InEvents, InCenter, false, 1);
			break;
		case 15:
			CheckPicking(!Editor.Selection, "navigation selected a model");
			Pointer(InEvents, InCenter, true);
			break;
		case 16:
		case 17:
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = Scenario.PickingExerciseStep == 17;
			InEvents.push_back(Focus);
			if (Focus.bDown)
			{
				Pointer(InEvents, InCenter, false);
			}
			break;
		}
		case 18:
			CheckPicking(!Editor.Selection, "focus loss completed a click");
			break;
		case 19:
			Pointer(InEvents, InCenter, true);
			break;
		case 20:
			Editor.Viewport.ViewCamera.World = Translation({1, 0, 10});
			break;
		case 21:
			Pointer(InEvents, InCenter, false);
			break;
		case 22:
			CheckPicking(!Editor.Selection, "view change completed a click");
			Editor.Viewport.ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
			break;
	}
}

void FEditorAcceptanceHarness::ExercisePickingView(std::vector<FInputEvent>& InEvents, FVec2 InCenter)
{
	switch (Scenario.PickingExerciseStep)
	{
		case 23:
			Editor.SetPreviewCamera(Scenario.PickingPreview);
			break;
		case 24:
			Pointer(InEvents, InCenter, true);
			break;
		case 25:
			Pointer(InEvents, InCenter, false);
			break;
		case 26:
			CheckPicking(Editor.Selection == Scenario.PickingNear, "preview camera picking failed");
			break;
		case 27:
			Editor.Scene->SetEnabled(Scenario.PickingPreview, false);
			Editor.SelectObject(Scenario.PickingFar);
			break;
		case 28:
			Pointer(InEvents, InCenter, true);
			break;
		case 29:
			Pointer(InEvents, InCenter, false);
			break;
		case 30:
			CheckPicking(Editor.Selection == Scenario.PickingFar,
			             "invalid preview cleared selection or used editor camera");
			Editor.Scene->SetEnabled(Scenario.PickingPreview, true);
			Editor.SelectObject(std::nullopt);
			break;
		case 31:
			Pointer(InEvents, InCenter, true);
			break;
		case 32:
			Editor.Window->Resize({1300, 800});
			break;
		case 34:
			Pointer(InEvents, InCenter, false);
			break;
		case 35:
			CheckPicking(!Editor.Selection, "resize completed a stale click");
			Editor.SetPreviewCamera(std::nullopt);
			Editor.SelectObject(Scenario.PickingFar);
			break;
		case 36:
			Pointer(InEvents, InCenter, true);
			break;
		case 37:
			CheckPicking(Editor.Gizmo.IsDragging(), "gizmo did not own center press");
			break;
		case 38:
			Pointer(InEvents, InCenter, false);
			break;
		case 39:
			CheckPicking(Editor.Selection == Scenario.PickingFar, "picking stole a gizmo gesture");
			CheckPicking(Editor.History.empty() && !Editor.IsDirty(), "interaction authored unintended edits");
			Editor.bShowLightMarkers = true;
			Editor.OpenScene(Editor.CurrentPath);
			break;
		case 40:
			CheckPicking(!Editor.Scene->Find(Scenario.PickingNear) && !Editor.ViewportClick,
			             "reopen retained stale picking state");
			Scenario.bPickingVerified = true;
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
	if (Scenario.PickingExerciseStep == 0)
	{
		PreparePickingExercise();
	}
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	const FVec2 Empty{Bounds.X + 10, Bounds.Y + 10};
	ExercisePickingSelection(InEvents, Center, Empty);
	ExercisePickingView(InEvents, Center);
	++Scenario.PickingExerciseStep;
}
} // namespace Hyperion
