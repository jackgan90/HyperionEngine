#include "EditorApplication.h"
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

void Pointer(std::vector<FInputEvent>& InEvents, FVec2 InPosition, bool bInDown, unsigned InButton = 0)
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

bool FEditorPlugin::ExercisePickingScene(std::vector<FInputEvent>& InEvents)
{
	if (PickingSceneStep > 10)
	{
		return true;
	}
	const auto Light = Scene->FindHandle("light-courtyard-3");
	CheckPicking(Light.Scene != 0, "requires Sponza courtyard light 3");
	const auto Bounds = ViewportRegion.Bounds;
	const FVec2 Surface{Bounds.X + (Bounds.Z - Bounds.X) * .2f, Bounds.Y + (Bounds.W - Bounds.Y) * .7f};
	const FVec2 LightRow{PickingLightBounds.X + 60, (PickingLightBounds.Y + PickingLightBounds.W) * .5f};
	switch (PickingSceneStep)
	{
		case 0:
			CheckPicking(PickingLightBounds.W > PickingLightBounds.Y, "light row is not visible");
			Pointer(InEvents, LightRow, true);
			break;
		case 2:
			Pointer(InEvents, LightRow, false);
			break;
		case 3:
		{
			CheckPicking(Selection == Light, "Outliner did not select courtyard light 3");
			CheckPicking(Gizmo.HitTest(Surface) == ETransformGizmoHandle::None, "surface overlaps light gizmo");
			const auto Ray = MakeViewportRay(ViewCamera, {.2f, .7f}, ViewportSize.Width, ViewportSize.Height,
			                                 EDepthConvention::Reversed);
			CheckPicking(Ray.has_value(), "Sponza view ray is unavailable");
			const auto QueryOptions = MakeSceneRayOptions(ESceneRenderPipeline::Deferred);
			const auto Hit = Scene->Raycast(*Ray, QueryOptions);
			CheckPicking(Hit.Status == ESceneRayStatus::Hit &&
			                 Hit.Handle == Scene->GetNodes(ESceneNodeKind::Model).front(),
			             "foreground Sponza geometry query failed");
			Pointer(InEvents, Surface, true);
			break;
		}
		case 7:
			CheckPicking(ViewportClick && ViewportRegion.bFocused && !Gizmo.IsDragging(),
			             "held Sponza click lost image ownership");
			break;
		case 8:
			Pointer(InEvents, Surface, false);
			break;
		case 9:
			CheckPicking(Selection == Scene->GetNodes(ESceneNodeKind::Model).front(),
			             "held Sponza click did not replace Outliner light selection");
			CheckPicking(History.empty() && !IsDirty(), "Sponza selection authored an edit");
			break;
		case 10:
			CheckPicking(Selection && Gizmo.InitialLocal().Values == Scene->FindNode(*Selection)->Local().Values,
			             "gizmo retained the light transform after selecting Sponza");
			break;
	}
	++PickingSceneStep;
	return false;
}

void FEditorPlugin::PreparePickingExercise()
{
	// This synthetic fixture isolates triangle picking; icon priority is exercised by placement acceptance.
	bShowLightMarkers = false;
	const auto Models = Scene->GetNodes(ESceneNodeKind::Model);
	CheckPicking(!Models.empty(), "requires a prepared scene");
	const auto Original = Scene->Find(Models.front())->Data;
	for (const auto Handle : Models)
	{
		Scene->SetModelVisible(Handle, false);
	}
	std::shared_ptr<const FSceneModelData> Data;
	Tasks.Wait(Tasks.Dispatch({EDomain::Worker},
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
	PickingFar = Scene->Add(Model);
	Model.Name = "Picking near";
	Model.World = Translation({0, 0, 1});
	PickingNear = Scene->Add(Model);
	ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
	PickingPreview = Scene->AddNode(MakeSceneCameraNode("picking-preview", {0, 0, 10}, {}));
	ResetDocument();
	SelectObject(std::nullopt);
}

void FEditorPlugin::ExercisePickingSelection(std::vector<FInputEvent>& InEvents, FVec2 InCenter, FVec2 InEmpty)
{
	switch (PickingExerciseStep)
	{
		case 2:
			Pointer(InEvents, InCenter, true);
			break;
		case 3:
			Pointer(InEvents, InCenter, false);
			break;
		case 4:
			CheckPicking(Selection == PickingNear,
			             "nearest triangle was not selected; selected " +
			                 (Selection ? Scene->FindNode(*Selection)->Id : std::string("nothing")));
			CheckPicking(History.empty() && !IsDirty(), "selection changed document history");
			break;
		case 5:
			Pointer(InEvents, InEmpty, true);
			break;
		case 6:
			Pointer(InEvents, InEmpty, false);
			break;
		case 7:
			CheckPicking(!Selection, "empty click did not clear selection");
			break;
		case 8:
			CheckPicking(!Selection, "outliner restored cleared selection");
			Pointer(InEvents, InCenter, true);
			break;
		case 9:
		case 10:
			Pointer(InEvents, {InCenter.X + 50, InCenter.Y}, PickingExerciseStep == 9);
			break;
		case 11:
			CheckPicking(!Selection, "drag selected a model");
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
			CheckPicking(!Selection, "navigation selected a model");
			Pointer(InEvents, InCenter, true);
			break;
		case 16:
		case 17:
		{
			FInputEvent Focus;
			Focus.Type = EEventType::Focus;
			Focus.bDown = PickingExerciseStep == 17;
			InEvents.push_back(Focus);
			if (Focus.bDown)
			{
				Pointer(InEvents, InCenter, false);
			}
			break;
		}
		case 18:
			CheckPicking(!Selection, "focus loss completed a click");
			break;
		case 19:
			Pointer(InEvents, InCenter, true);
			break;
		case 20:
			ViewCamera.World = Translation({1, 0, 10});
			break;
		case 21:
			Pointer(InEvents, InCenter, false);
			break;
		case 22:
			CheckPicking(!Selection, "view change completed a click");
			ViewCamera.World = SceneCameraTransform({0, 0, 10}, {});
			break;
	}
}

void FEditorPlugin::ExercisePickingView(std::vector<FInputEvent>& InEvents, FVec2 InCenter)
{
	switch (PickingExerciseStep)
	{
		case 23:
			SetPreviewCamera(PickingPreview);
			break;
		case 24:
			Pointer(InEvents, InCenter, true);
			break;
		case 25:
			Pointer(InEvents, InCenter, false);
			break;
		case 26:
			CheckPicking(Selection == PickingNear, "preview camera picking failed");
			break;
		case 27:
			Scene->SetEnabled(PickingPreview, false);
			SelectObject(PickingFar);
			break;
		case 28:
			Pointer(InEvents, InCenter, true);
			break;
		case 29:
			Pointer(InEvents, InCenter, false);
			break;
		case 30:
			CheckPicking(Selection == PickingFar, "invalid preview cleared selection or used editor camera");
			Scene->SetEnabled(PickingPreview, true);
			SelectObject(std::nullopt);
			break;
		case 31:
			Pointer(InEvents, InCenter, true);
			break;
		case 32:
			Window->Resize({1300, 800});
			break;
		case 34:
			Pointer(InEvents, InCenter, false);
			break;
		case 35:
			CheckPicking(!Selection, "resize completed a stale click");
			SetPreviewCamera(std::nullopt);
			SelectObject(PickingFar);
			break;
		case 36:
			Pointer(InEvents, InCenter, true);
			break;
		case 37:
			CheckPicking(Gizmo.IsDragging(), "gizmo did not own center press");
			break;
		case 38:
			Pointer(InEvents, InCenter, false);
			break;
		case 39:
			CheckPicking(Selection == PickingFar, "picking stole a gizmo gesture");
			CheckPicking(History.empty() && !IsDirty(), "interaction authored unintended edits");
			bShowLightMarkers = true;
			OpenScene(CurrentPath);
			break;
		case 40:
			CheckPicking(!Scene->Find(PickingNear) && !ViewportClick, "reopen retained stale picking state");
			bPickingVerified = true;
			break;
	}
}

void FEditorPlugin::ExercisePickingInput(std::vector<FInputEvent>& InEvents)
{
	if (ReadyFrames < 10 || !bViewportVisible)
	{
		return;
	}
	if (!ExercisePickingScene(InEvents))
	{
		return;
	}
	if (PickingExerciseStep == 0)
	{
		PreparePickingExercise();
	}
	const auto Bounds = ViewportRegion.Bounds;
	const FVec2 Center{(Bounds.X + Bounds.Z) / 2, (Bounds.Y + Bounds.W) / 2};
	const FVec2 Empty{Bounds.X + 10, Bounds.Y + 10};
	ExercisePickingSelection(InEvents, Center, Empty);
	ExercisePickingView(InEvents, Center);
	++PickingExerciseStep;
}
} // namespace Hyperion
