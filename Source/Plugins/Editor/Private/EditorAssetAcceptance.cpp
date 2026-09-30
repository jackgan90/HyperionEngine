#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include <array>

namespace Hyperion
{
namespace
{
void CheckAsset(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

void Shortcut(std::vector<FInputEvent>& InEvents, EKey InKey)
{
	FInputEvent Key;
	Key.Type = EEventType::Key;
	Key.Key = InKey;
	Key.Modifiers = InputModifiers::Control;
	Key.bDown = true;
	InEvents.push_back(Key);
	Key.bDown = false;
	Key.Modifiers = 0;
	InEvents.push_back(Key);
}

constexpr std::array Names{"Texture", "Model", "Sky", "Material", "CustomMaterial"};
} // namespace

void FEditorPlugin::ExerciseAssetInput(std::vector<FInputEvent>& InEvents)
{
	if (!Scene->GetStatus().Error.empty())
	{
		throw std::runtime_error(Scene->GetStatus().Error);
	}
	for (const auto& Asset : Scene->GetAssets())
	{
		if (!Asset.Error.empty())
		{
			throw std::runtime_error(Asset.Error);
		}
	}
	for (const auto Handle : Scene->GetHandles())
	{
		if (!Scene->GetError(Handle).empty())
		{
			throw std::runtime_error(Scene->GetError(Handle));
		}
	}
	if (!Scene->GetStatus().AssetRefreshError.empty())
	{
		throw std::runtime_error(Scene->GetStatus().AssetRefreshError);
	}
	if (Acceptance.ExerciseStep >= 190)
	{
		ExerciseAssetWindowInput(InEvents);
		return;
	}
	if (Acceptance.ExerciseStep >= 100)
	{
		ExerciseAssetPropertyInput(InEvents);
		return;
	}
	const auto Name = std::string(Names.at(Acceptance.AssetExerciseIndex));
	if (Acceptance.AssetExerciseLoggedStep != static_cast<int>(Acceptance.ExerciseStep))
	{
		Acceptance.AssetExerciseLoggedStep = static_cast<int>(Acceptance.ExerciseStep);
		Log(ELogLevel::Info, "Asset acceptance " + Name + " step " + std::to_string(Acceptance.ExerciseStep));
	}
	if (Acceptance.ExerciseStep <= 12)
	{
		ExerciseAssetOpening(InEvents);
	}
	else if (Acceptance.ExerciseStep <= 18)
	{
		ExerciseAssetNameInput(InEvents);
	}
	else
	{
		ExerciseAssetSaving(InEvents);
	}
}

void FEditorPlugin::ExerciseAssetOpening(std::vector<FInputEvent>& InEvents)
{
	const auto Name = std::string(Names.at(Acceptance.AssetExerciseIndex));
	const auto Path = "/Game/" + Name + ".hasset";
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (Acceptance.ExerciseStep)
	{
		case 0:
			QueueContentRoot(Options.ExerciseAssets);
			++Acceptance.ExerciseStep;
			break;
		case 1:
			RequestOpenAsset("/Game/Scene.hasset");
			++Acceptance.ExerciseStep;
			break;
		case 2:
			if (Scene->GetStatus().bReady && ReadyFrames > 8)
			{
				const auto Handle = Scene->FindHandle("model");
				auto Node = *Scene->FindNode(Handle);
				Node.Name = "Unsaved scene edit";
				Node.Model()->Material.Roughness = .73f;
				CommitEdit(Handle, Node, Scene->GetRevision());
				Selection = Handle;
				Acceptance.ExerciseStep = 9;
			}
			break;
		case 9:
			ContentRevealPath = Path;
			++Acceptance.ExerciseStep;
			break;
		case 10:
		case 11:
			if (ContentTileBounds.contains(Path))
			{
				ExerciseClick(InEvents, ContentTileBounds.at(Path));
			}
			break;
		case 12:
			if (++Acceptance.ExerciseWait == 120)
			{
				Log(ELogLevel::Info, "Waiting for " + Path + ": " + AssetWorkspace->ActiveStatus());
				Log(ELogLevel::Info,
				    "Content selection: " + Browser->SelectedFile + "; open request: " + AssetOpenPath);
				Acceptance.PlacementCapture =
				    Options.ExerciseAssets.parent_path() / ("AssetEditor-" + Name + "-Waiting.png");
				Acceptance.OutlineCapture =
				    Options.ExerciseAssets.parent_path() / ("AssetWindow-" + Name + "-Waiting.png");
			}
			if (Document && PathToUtf8(Document->Loaded().Path) == Path && AssetWorkspace->IsPreviewReady())
			{
				Acceptance.AssetExerciseOriginalName = ReadValue<std::string>(Document->Get("name"));
				CheckAsset(!Document->IsDirty(), "Opening an asset marked it dirty");
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
			}
			break;
		default:
			break;
	}
}

void FEditorPlugin::ExerciseAssetNameInput(std::vector<FInputEvent>& InEvents)
{
	const auto Name = std::string(Names.at(Acceptance.AssetExerciseIndex));
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (Acceptance.ExerciseStep)
	{
		case 13:
			ExerciseClick(InEvents, AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 14:
		{
			++Acceptance.ExerciseWait;
			if (Acceptance.ExerciseWait <= 2)
			{
				FInputEvent Key;
				Key.Type = EEventType::Key;
				Key.Key = EKey::A;
				Key.bDown = Acceptance.ExerciseWait == 1;
				Key.Modifiers = Key.bDown ? 1 : 0;
				InEvents.push_back(Key);
				if (Acceptance.ExerciseWait == 2)
				{
					FInputEvent Text;
					Text.Type = EEventType::Text;
					Text.Text = "Edited " + Name;
					InEvents.push_back(Text);
				}
			}
			if (Acceptance.ExerciseWait == 4)
			{
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
			}
			break;
		}
		case 15:
			CheckAsset(AssetWorkspace->HasActiveInteraction(), "Name input lost its active live-edit interaction");
			Acceptance.PlacementCapture = Options.ExerciseAssets.parent_path() / ("AssetEditor-" + Name + "-Input.png");
			++Acceptance.ExerciseStep;
			break;
		case 16:
			CheckAsset(Document && Document->IsDirty() &&
			               ReadValue<std::string>(Document->Get("name")) == "Edited " + Name,
			           "Asset name input did not edit the document");
			Shortcut(InEvents, EKey::Z);
			++Acceptance.ExerciseStep;
			break;
		case 17:
			CheckAsset(Document && !Document->IsDirty() &&
			               ReadValue<std::string>(Document->Get("name")) == Acceptance.AssetExerciseOriginalName,
			           "Asset Undo did not restore baseline");
			Shortcut(InEvents, EKey::Y);
			++Acceptance.ExerciseStep;
			break;
		case 18:
			CheckAsset(Document && Document->IsDirty(), "Asset Redo did not restore the edit");
			Shortcut(InEvents, EKey::S);
			++Acceptance.ExerciseStep;
			break;
		default:
			break;
	}
}

void FEditorPlugin::ExerciseAssetSaving(std::vector<FInputEvent>& InEvents)
{
	const auto Name = std::string(Names.at(Acceptance.AssetExerciseIndex));
	const auto Path = "/Game/" + Name + ".hasset";
	const auto* Document = AssetWorkspace->ActiveDocument();
	switch (Acceptance.ExerciseStep)
	{
		case 19:
			if (Document && !Document->IsSaving() && !Document->IsDirty() && AssetWorkspace->IsPreviewReady())
			{
				CheckAsset(Document->Error.empty(), "Asset save failed");
				const auto Saved = Assets.LoadAsync(Path).Get(Tasks);
				const auto Node = WriteRecord(*Saved->Type, Saved->Object.get());
				const auto& Fields =
				    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Node.Value).at("fields").Value);
				CheckAsset(ReadValue<std::string>(Fields.at("name")) == "Edited " + Name,
				           "Saved asset did not retain property edit");
				CheckAsset(IsDirty() && History.size() == 1 &&
				               Scene->FindNode(Scene->FindHandle("model"))->Name == "Unsaved scene edit",
				           "Asset save discarded scene edits or history");
				Acceptance.PlacementCapture = Options.ExerciseAssets.parent_path() / ("AssetEditor-" + Name + ".png");
				if (Name == "Material")
				{
					Acceptance.OutlineCapture = Options.ExerciseAssets.parent_path() / "AssetWindow-Scene.png";
				}
				++Acceptance.ExerciseStep;
			}
			break;
		case 20:
			if (++Acceptance.AssetExerciseIndex < Names.size())
			{
				Acceptance.ExerciseStep = 9;
			}
			else
			{
				Acceptance.AssetExerciseIndex = Names.size() - 1;
				Acceptance.ExerciseStep = 100;
			}
			break;
		case 21:
			Undo();
			CheckAsset(!IsDirty(), "Scene undo baseline was lost after asset saves");
			Redo();
			CheckAsset(IsDirty() && Scene->FindNode(Scene->FindHandle("model"))->Name == "Unsaved scene edit",
			           "Scene redo was lost after asset saves");
			Window->RequestClose();
			++Acceptance.ExerciseStep;
			break;
		case 22:
			ExerciseClick(InEvents, CancelChangesBounds);
			break;
		case 23:
			CheckAsset(!Window->ShouldClose() && IsDirty(), "Cancel close lost workspace changes");
			Acceptance.bAssetsVerified = true;
			Log(ELogLevel::Info, "Asset editors: five previews, native save, undo/redo, scene preservation and close "
			                     "cancellation passed");
			ResetDocument();
			Window->RequestClose();
			++Acceptance.ExerciseStep;
			break;
		default:
			break;
	}
}
} // namespace Hyperion
