#include "EditorAcceptanceHarness.h"
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

void FEditorAcceptanceHarness::ExerciseModelPlacement(std::vector<FInputEvent>& InEvents)
{
	CheckModelPlacement(!Editor.bAssetMessage, Editor.AssetMessage);
	if (Scenario.ModelPlacementStep == 0 && Scenario.ModelPlacementCase == 0)
	{
		const auto Root = Editor.Options.ExerciseModelPlacement / "Game";
		std::filesystem::create_directories(Root);
		WritePlacementFixture(Root);
		Editor.QueueContentRoot(Root);
		FInputEvent Focus;
		Focus.Type = EEventType::Focus;
		Focus.bDown = true;
		InEvents.push_back(Focus);
		Scenario.ModelPlacementStep = 1;
		return;
	}
	if (Scenario.ExerciseWait)
	{
		--Scenario.ExerciseWait;
		return;
	}
	if (Editor.Transition.HasPendingRoot() || Editor.Browser->IsScanning() || Editor.FrameCount < 12 ||
	    !Editor.Viewport.bViewportVisible || !Editor.Scene->GetStatus().bReady)
	{
		return;
	}
	if (Scenario.ModelPlacementCase < ModelPlacementNames.size())
	{
		ExerciseModelDrag(InEvents);
	}
	else
	{
		ExerciseModelPlacementHistory();
	}
}

void FEditorAcceptanceHarness::ExerciseModelDrag(std::vector<FInputEvent>& InEvents)
{
	const std::string Path = "/Game/" + std::string(ModelPlacementNames.at(Scenario.ModelPlacementCase)) + ".hasset";
	if (!Scenario.ContentTileBounds.contains(Path))
	{
		return;
	}
	const auto Source = Scenario.ContentTileBounds.at(Path);
	const FVec2 Start{(Source.X + Source.Z) * .5f, (Source.Y + Source.W) * .5f};
	const auto Bounds = Editor.Viewport.ViewportRegion.Bounds;
	const FVec2 Target{Bounds.X + (Bounds.Z - Bounds.X) * .55f, Bounds.Y + (Bounds.W - Bounds.Y) * .65f};
	if (Scenario.ModelPlacementStep >= 7)
	{
		CompleteModelDrag(InEvents, Start);
		return;
	}
	switch (Scenario.ModelPlacementStep)
	{
		case 1:
			Scenario.ModelPlacementBaseNodes = Editor.Scene->GetNodes().size();
			Scenario.ModelPlacementBaseHistory = Editor.HistoryCursor;
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
			if (Scenario.ModelPlacementCase == 0)
			{
				ModelButton(InEvents, false);
				Scenario.ModelPlacementStep = 8;
				return;
			}
			break;
		case 5:
			CheckModelPlacement(Editor.Gui->DragPayload().has_value(), "Content tile did not start a drag in case " +
			                                                               std::to_string(Scenario.ModelPlacementCase));
			if (Scenario.ModelPlacementCase >= 4 && Scenario.ModelPlacementCase <= 6)
			{
				if (Editor.PlacementPreparation.State == EPlacementPreparationState::Pending)
				{
					return;
				}
				CheckModelPlacement(!Editor.Placement.GetPreview() &&
				                        Editor.PlacementPreparation.State == EPlacementPreparationState::Failed,
				                    "Invalid asset accepted");
				ModelButton(InEvents, false);
				Scenario.ModelPlacementStep = 8;
				return;
			}
			if (!Editor.Placement.GetPreview())
			{
				return;
			}
			{
				const auto Preview = Editor.FreezePlacementPreview();
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
			CheckModelPlacement(Editor.HistoryCursor == Scenario.ModelPlacementBaseHistory &&
			                        Editor.Scene->GetNodes().size() == Scenario.ModelPlacementBaseNodes,
			                    "Preview modified document");
			MoveModelPointer(InEvents, Start);
			break;
		case 6:
			CheckModelPlacement(Editor.Placement.IsActive() && !Editor.Placement.GetPreview(),
			                    "Outside preview was not hidden");
			MoveModelPointer(InEvents, Target);
			Scenario.PlacementCapture = Editor.Options.ExerciseModelPlacement / "ModelPreview.png";
			break;
	}
	++Scenario.ModelPlacementStep;
}

void FEditorAcceptanceHarness::CompleteModelDrag(std::vector<FInputEvent>& InEvents, FVec2 InSource)
{
	if (Scenario.ModelPlacementStep == 7)
	{
		CheckModelPlacement(Editor.Placement.GetPreview().has_value(), "Returning drag lost preview");
		Scenario.ModelPlacementPosition = Editor.Placement.GetPreview()->Position;
		bool bBusyRejected{};
		try
		{
			Editor.PlaceModel({Editor.SceneDocument.Id(), Editor.Scene->GetRevision(),
			                   *Editor.PlacementCandidate->Model, Scenario.ModelPlacementPosition});
		}
		catch (const FSceneEditError& Failure)
		{
			bBusyRejected = Failure.Code == "busy";
		}
		CheckModelPlacement(bBusyRejected, "Automation did not reject placement during a GUI gesture");
		if (Scenario.ModelPlacementCase == 3)
		{
			FInputEvent Cancel;
			Cancel.Type = EEventType::Key;
			Cancel.Key = EKey::Escape;
			Cancel.bDown = true;
			InEvents.push_back(Cancel);
		}
		else if (Scenario.ModelPlacementCase == 7)
		{
			MoveModelPointer(InEvents, InSource);
		}
		else if (Scenario.ModelPlacementCase == 8)
		{
			Editor.Viewport.ViewCamera.World.Values[12] += 1;
		}
		ModelButton(InEvents, false);
		Scenario.ModelPlacementStep = 8;
		return;
	}
	if (Scenario.ModelPlacementCase == 3)
	{
		FInputEvent Release;
		Release.Type = EEventType::Key;
		Release.Key = EKey::Escape;
		InEvents.push_back(Release);
	}
	CheckModelPlacement(!Editor.Placement.IsActive(), "Drop/cancellation left an active gesture");
	if (Scenario.ModelPlacementCase == 1 || Scenario.ModelPlacementCase == 2)
	{
		CheckModelPlacement(Editor.HistoryCursor == Scenario.ModelPlacementBaseHistory + 1 &&
		                        Editor.Scene->GetNodes().size() == Scenario.ModelPlacementBaseNodes + 1,
		                    "Drop did not create exactly one history entry and node");
		const auto* Node = Editor.Scene->FindNode(*Editor.Selection);
		CheckModelPlacement(Node && Node->Name == "Model" && Node->Model() && Editor.Gui->IsWindowFocused("Viewport"),
		                    "Drop selection/name/focus mismatch");
		CheckModelPlacement(Node->Local().Values == Translation(Scenario.ModelPlacementPosition).Values,
		                    "Preview/commit transform mismatch");
		Scenario.ModelPlacementIds.push_back(Node->Id);
	}
	else
	{
		CheckModelPlacement(Editor.HistoryCursor == Scenario.ModelPlacementBaseHistory &&
		                        Editor.Scene->GetNodes().size() == Scenario.ModelPlacementBaseNodes,
		                    "Cancelled or invalid drop changed document");
	}
	++Scenario.ModelPlacementCase;
	if (Scenario.ModelPlacementCase < ModelPlacementNames.size())
	{
		Scenario.ContentRevealPath =
		    "/Game/" + std::string(ModelPlacementNames.at(Scenario.ModelPlacementCase)) + ".hasset";
	}
	Scenario.ModelPlacementStep = 1;
	Scenario.ExerciseWait = 24;
}

void FEditorAcceptanceHarness::ExerciseModelPlacementHistory()
{
	const auto Output = Editor.Options.ExerciseModelPlacement / "Placed.hasset";
	if (Scenario.ModelPlacementStep == 1)
	{
		const auto* First = Editor.Scene->FindNode(Editor.Scene->FindHandle(Scenario.ModelPlacementIds.at(0)));
		const auto* Second = Editor.Scene->FindNode(Editor.Scene->FindHandle(Scenario.ModelPlacementIds.at(1)));
		CheckModelPlacement(First->Model()->Asset == Second->Model()->Asset &&
		                        First->Model()->Data == Second->Model()->Data,
		                    "Repeated placement did not share model data");
		CheckModelPlacement(First->Model()->Data->Instances.size() == 2 && !First->Model()->Data->Materials.empty(),
		                    "Model instances/materials lost");
		Editor.Undo();
		CheckModelPlacement(Editor.Scene->GetNodes().size() == 1, "Model placement undo failed");
		Editor.Redo();
		CheckModelPlacement(Editor.Scene->GetNodes().size() == 2 && bool(Editor.Selection),
		                    "Model placement redo failed");
		const auto Snapshot = Editor.Scene->Snapshot(Output);
		CheckModelPlacement(Snapshot.Assets.size() == 1, "Cancelled/failed models leaked into saved references");
		Editor.SaveScene(PathToUtf8(Output));
		++Scenario.ModelPlacementStep;
	}
	else if (Scenario.ModelPlacementStep == 2 && !Editor.PendingSave)
	{
		CheckModelPlacement(!Editor.IsDirty(), "Model placement save failed");
		Editor.OpenScene(PathToUtf8(Output));
		++Scenario.ModelPlacementStep;
	}
	else if (Scenario.ModelPlacementStep == 3)
	{
		CheckModelPlacement(Editor.Scene->GetNodes().size() == 2 && !Editor.IsDirty(),
		                    "Saved model scene did not reload");
		for (const auto& Id : Scenario.ModelPlacementIds)
		{
			const auto* Node = Editor.Scene->FindNode(Editor.Scene->FindHandle(Id));
			CheckModelPlacement(Node && Node->Model()->Data->Instances.size() == 2, "Reload lost model or instances");
		}
		Scenario.bModelPlacementVerified = true;
	}
}
} // namespace Hyperion
