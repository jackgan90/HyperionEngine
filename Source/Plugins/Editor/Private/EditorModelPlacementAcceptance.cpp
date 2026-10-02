#include "EditorApplication.h"
#include <fstream>

namespace Hyperion
{
namespace
{
constexpr std::array ModelPlacementNames{"Model",  "Model",  "Model", "Second", "Texture",
                                         "Broken", "Failed", "Model", "Model"};

void CheckModelPlacement(bool bInCondition, const std::string& InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error("Model placement acceptance: " + InMessage);
	}
}

void MoveModelPointer(std::vector<FInputEvent>& InEvents, FVec2 InPoint)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseMove;
	Event.X = InPoint.X;
	Event.Y = InPoint.Y;
	InEvents.push_back(Event);
}

void ModelButton(std::vector<FInputEvent>& InEvents, bool bInDown)
{
	FInputEvent Event;
	Event.Type = EEventType::MouseButton;
	Event.bDown = bInDown;
	InEvents.push_back(Event);
}

void WritePlacementFixture(const std::filesystem::path& InRoot)
{
	FLocalFileSystem Files;
	const auto Engine = std::filesystem::path(HYP_SOURCE_DIR) / "Content";
	auto Model = ReadValue<FModelAsset>(
	    DecodeAsset(Files.Read(Engine / "Models/Primitives/Cube.hasset", 16 * 1024 * 1024)).Object);
	Model.Nodes = {{"Left", Translation({-1, 0, 0}), {0}, {}, "left"},
	               {"Right", Translation({1, 1, 0}), {0}, {}, "right"}};
	Model.Roots = {0, 1};
	for (const auto* Name : {"Model", "Second"})
	{
		Model.Name = Name;
		Files.WriteAtomic(InRoot / (std::string(Name) + ".hasset"),
		                  EncodeAsset(RecordType<FModelAsset>(), &Model).Bytes);
	}
	Model.MaterialSlots = {{"", "/Game/MissingMaterial.hasset", RecordType<FMaterialAsset>().Id, ""}};
	Files.WriteAtomic(InRoot / "Failed.hasset", EncodeAsset(RecordType<FModelAsset>(), &Model).Bytes);
	const auto Texture = ReadValue<FTextureAsset>(
	    DecodeAsset(Files.Read(Engine / "Editor/Icons/PointLight.hasset", 16 * 1024 * 1024)).Object);
	Files.WriteAtomic(InRoot / "Texture.hasset", EncodeAsset(RecordType<FTextureAsset>(), &Texture).Bytes);
	std::ofstream(InRoot / "Broken.hasset") << "invalid native asset";
}
} // namespace

void FEditorPlugin::ExerciseModelPlacement(std::vector<FInputEvent>& InEvents)
{
	CheckModelPlacement(!bAssetMessage, AssetMessage);
	if (Acceptance.ModelPlacementStep == 0 && Acceptance.ModelPlacementCase == 0)
	{
		const auto Root = Options.ExerciseModelPlacement / "Game";
		std::filesystem::create_directories(Root);
		WritePlacementFixture(Root);
		QueueContentRoot(Root);
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = true;
		InEvents.push_back(Focus);
		Acceptance.ModelPlacementStep = 1;
		return;
	}
	if (Acceptance.ExerciseWait)
	{
		--Acceptance.ExerciseWait;
		return;
	}
	if (Transition.PendingRoot || Browser->IsScanning() || FrameCount < 12 || !Viewport.bViewportVisible ||
	    !Scene->GetStatus().bReady)
	{
		return;
	}
	if (Acceptance.ModelPlacementCase < ModelPlacementNames.size())
	{
		ExerciseModelDrag(InEvents);
	}
	else
	{
		ExerciseModelPlacementHistory();
	}
}

void FEditorPlugin::ExerciseModelDrag(std::vector<FInputEvent>& InEvents)
{
	const std::string Path = "/Game/" + std::string(ModelPlacementNames.at(Acceptance.ModelPlacementCase)) + ".hasset";
	if (!ContentTileBounds.contains(Path))
	{
		return;
	}
	const auto Source = ContentTileBounds.at(Path);
	const FVec2 Start{(Source.X + Source.Z) * .5f, (Source.Y + Source.W) * .5f};
	const auto Bounds = Viewport.ViewportRegion.Bounds;
	const FVec2 Target{Bounds.X + (Bounds.Z - Bounds.X) * .55f, Bounds.Y + (Bounds.W - Bounds.Y) * .65f};
	if (Acceptance.ModelPlacementStep >= 7)
	{
		CompleteModelDrag(InEvents, Start);
		return;
	}
	switch (Acceptance.ModelPlacementStep)
	{
		case 1:
			Acceptance.ModelPlacementBaseNodes = Scene->GetNodes().size();
			Acceptance.ModelPlacementBaseHistory = HistoryCursor;
			MoveModelPointer(InEvents, Start);
			break;
		case 2:
			ModelButton(InEvents, true);
			break;
		case 3:
			MoveModelPointer(InEvents, {Start.X + 15, Start.Y});
			break;
		case 4:
			MoveModelPointer(InEvents, Target);
			if (Acceptance.ModelPlacementCase == 0)
			{
				ModelButton(InEvents, false);
				Acceptance.ModelPlacementStep = 8;
				return;
			}
			break;
		case 5:
			CheckModelPlacement(Gui->DragPayload().has_value(), "Content tile did not start a drag in case " +
			                                                        std::to_string(Acceptance.ModelPlacementCase));
			if (Acceptance.ModelPlacementCase >= 4 && Acceptance.ModelPlacementCase <= 6)
			{
				if (PlacementPreparation.State == EPlacementPreparationState::Pending)
				{
					return;
				}
				CheckModelPlacement(!Placement.GetPreview() &&
				                        PlacementPreparation.State == EPlacementPreparationState::Failed,
				                    "Invalid asset accepted");
				ModelButton(InEvents, false);
				Acceptance.ModelPlacementStep = 8;
				return;
			}
			if (!Placement.GetPreview())
			{
				return;
			}
			{
				const auto Preview = FreezePlacementPreview();
				CheckModelPlacement(Preview->Items.size() + Preview->SceneItems.size() == 2,
				                    "Missing model instances in preview");
				if (Preview->SceneItems.size() != 2)
				{
					return;
				}
				for (const auto& Item : Preview->SceneItems)
				{
					CheckModelPlacement(Item.Surface == Item.Resource->GetMaterial(Item.Section),
					                    "Preview did not use the source material");
				}
			}
			CheckModelPlacement(HistoryCursor == Acceptance.ModelPlacementBaseHistory &&
			                        Scene->GetNodes().size() == Acceptance.ModelPlacementBaseNodes,
			                    "Preview modified document");
			MoveModelPointer(InEvents, Start);
			break;
		case 6:
			CheckModelPlacement(Placement.IsActive() && !Placement.GetPreview(), "Outside preview was not hidden");
			MoveModelPointer(InEvents, Target);
			Acceptance.PlacementCapture = Options.ExerciseModelPlacement / "ModelPreview.png";
			break;
	}
	++Acceptance.ModelPlacementStep;
}

void FEditorPlugin::CompleteModelDrag(std::vector<FInputEvent>& InEvents, FVec2 InSource)
{
	if (Acceptance.ModelPlacementStep == 7)
	{
		CheckModelPlacement(Placement.GetPreview().has_value(), "Returning drag lost preview");
		Acceptance.ModelPlacementPosition = Placement.GetPreview()->Position;
		bool bBusyRejected{};
		try
		{
			PlaceModel({SceneDocument.Id(), Scene->GetRevision(), *PlacementCandidate->Model,
			            Acceptance.ModelPlacementPosition});
		}
		catch (const FSceneEditError& Failure)
		{
			bBusyRejected = Failure.Code == "busy";
		}
		CheckModelPlacement(bBusyRejected, "Automation did not reject placement during a GUI gesture");
		if (Acceptance.ModelPlacementCase == 3)
		{
			FInputEvent Cancel;
			Cancel.Type = EEventType::Key;
			Cancel.Key = EKey::Escape;
			Cancel.bDown = true;
			InEvents.push_back(Cancel);
		}
		else if (Acceptance.ModelPlacementCase == 7)
		{
			MoveModelPointer(InEvents, InSource);
		}
		else if (Acceptance.ModelPlacementCase == 8)
		{
			Viewport.ViewCamera.World.Values[12] += 1;
		}
		ModelButton(InEvents, false);
		Acceptance.ModelPlacementStep = 8;
		return;
	}
	if (Acceptance.ModelPlacementCase == 3)
	{
		FInputEvent Release;
		Release.Type = EEventType::Key;
		Release.Key = EKey::Escape;
		InEvents.push_back(Release);
	}
	CheckModelPlacement(!Placement.IsActive(), "Drop/cancellation left an active gesture");
	if (Acceptance.ModelPlacementCase == 1 || Acceptance.ModelPlacementCase == 2)
	{
		CheckModelPlacement(HistoryCursor == Acceptance.ModelPlacementBaseHistory + 1 &&
		                        Scene->GetNodes().size() == Acceptance.ModelPlacementBaseNodes + 1,
		                    "Drop did not create exactly one history entry and node");
		const auto* Node = Scene->FindNode(*Selection);
		CheckModelPlacement(Node && Node->Name == "Model" && Node->Model() && Gui->IsWindowFocused("Viewport"),
		                    "Drop selection/name/focus mismatch");
		CheckModelPlacement(Node->Local().Values == Translation(Acceptance.ModelPlacementPosition).Values,
		                    "Preview/commit transform mismatch");
		Acceptance.ModelPlacementIds.push_back(Node->Id);
	}
	else
	{
		CheckModelPlacement(HistoryCursor == Acceptance.ModelPlacementBaseHistory &&
		                        Scene->GetNodes().size() == Acceptance.ModelPlacementBaseNodes,
		                    "Cancelled or invalid drop changed document");
	}
	++Acceptance.ModelPlacementCase;
	if (Acceptance.ModelPlacementCase < ModelPlacementNames.size())
	{
		ContentRevealPath = "/Game/" + std::string(ModelPlacementNames.at(Acceptance.ModelPlacementCase)) + ".hasset";
	}
	Acceptance.ModelPlacementStep = 1;
	Acceptance.ExerciseWait = 24;
}

void FEditorPlugin::ExerciseModelPlacementHistory()
{
	const auto Output = Options.ExerciseModelPlacement / "Placed.hasset";
	if (Acceptance.ModelPlacementStep == 1)
	{
		const auto* First = Scene->FindNode(Scene->FindHandle(Acceptance.ModelPlacementIds.at(0)));
		const auto* Second = Scene->FindNode(Scene->FindHandle(Acceptance.ModelPlacementIds.at(1)));
		CheckModelPlacement(First->Model()->Asset == Second->Model()->Asset &&
		                        First->Model()->Data == Second->Model()->Data,
		                    "Repeated placement did not share model data");
		CheckModelPlacement(First->Model()->Data->Instances.size() == 2 && !First->Model()->Data->Materials.empty(),
		                    "Model instances/materials lost");
		Undo();
		CheckModelPlacement(Scene->GetNodes().size() == 1, "Model placement undo failed");
		Redo();
		CheckModelPlacement(Scene->GetNodes().size() == 2 && bool(Selection), "Model placement redo failed");
		const auto Snapshot = Scene->Snapshot(Output);
		CheckModelPlacement(Snapshot.Assets.size() == 1, "Cancelled/failed models leaked into saved references");
		SaveScene(PathToUtf8(Output));
		++Acceptance.ModelPlacementStep;
	}
	else if (Acceptance.ModelPlacementStep == 2 && !PendingSave)
	{
		CheckModelPlacement(!IsDirty(), "Model placement save failed");
		OpenScene(PathToUtf8(Output));
		++Acceptance.ModelPlacementStep;
	}
	else if (Acceptance.ModelPlacementStep == 3)
	{
		CheckModelPlacement(Scene->GetNodes().size() == 2 && !IsDirty(), "Saved model scene did not reload");
		for (const auto& Id : Acceptance.ModelPlacementIds)
		{
			const auto* Node = Scene->FindNode(Scene->FindHandle(Id));
			CheckModelPlacement(Node && Node->Model()->Data->Instances.size() == 2, "Reload lost model or instances");
		}
		Acceptance.bModelPlacementVerified = true;
	}
}
} // namespace Hyperion
