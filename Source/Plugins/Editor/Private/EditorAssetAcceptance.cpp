#include "EditorAcceptanceHarness.h"
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

void FEditorAcceptanceHarness::ExerciseAssetInput(std::vector<FInputEvent>& InEvents)
{
	if (!Editor.Scene->GetStatus().Error.empty())
	{
		throw std::runtime_error(Editor.Scene->GetStatus().Error);
	}
	for (const auto& Asset : Editor.Scene->GetAssets())
	{
		if (!Asset.Error.empty())
		{
			throw std::runtime_error(Asset.Error);
		}
	}
	for (const auto Handle : Editor.Scene->GetHandles())
	{
		if (!Editor.Scene->GetError(Handle).empty())
		{
			throw std::runtime_error(Editor.Scene->GetError(Handle));
		}
	}
	if (!Editor.Scene->GetStatus().AssetRefreshError.empty())
	{
		throw std::runtime_error(Editor.Scene->GetStatus().AssetRefreshError);
	}
	if (Scenario.ExerciseStep >= 190)
	{
		ExerciseAssetWindowInput(InEvents);
		return;
	}
	if (Scenario.ExerciseStep >= 100)
	{
		ExerciseAssetPropertyInput(InEvents);
		return;
	}
	const auto Name = std::string(Names.at(Scenario.AssetExerciseIndex));
	if (Scenario.AssetExerciseLoggedStep != static_cast<int>(Scenario.ExerciseStep))
	{
		Scenario.AssetExerciseLoggedStep = static_cast<int>(Scenario.ExerciseStep);
		Log(ELogLevel::Info, "Asset acceptance " + Name + " step " + std::to_string(Scenario.ExerciseStep));
	}
	if (Scenario.ExerciseStep <= 12)
	{
		ExerciseAssetOpening(InEvents);
	}
	else if (Scenario.ExerciseStep <= 18)
	{
		ExerciseAssetNameInput(InEvents);
	}
	else
	{
		ExerciseAssetSaving(InEvents);
	}
}

void FEditorAcceptanceHarness::ExerciseAssetOpening(std::vector<FInputEvent>& InEvents)
{
	const auto Name = std::string(Names.at(Scenario.AssetExerciseIndex));
	const auto Path = "/Game/" + Name + ".hasset";
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 0:
			Editor.QueueContentRoot(Editor.Options.ExerciseAssets);
			++Scenario.ExerciseStep;
			break;
		case 1:
			Editor.RequestOpenAsset("/Game/Scene.hasset");
			++Scenario.ExerciseStep;
			break;
		case 2:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				const auto Handle = Editor.Scene->FindHandle("model");
				auto Node = *Editor.Scene->FindNode(Handle);
				Node.Name = "Unsaved scene edit";
				Node.Model()->Material.Roughness = .73f;
				Editor.CommitEdit(Handle, Node, Editor.Scene->GetRevision());
				Editor.Selection = Handle;
				Scenario.ExerciseStep = 9;
			}
			break;
		case 9:
			Scenario.ContentRevealPath = Path;
			++Scenario.ExerciseStep;
			break;
		case 10:
		case 11:
			if (Scenario.ContentTileBounds.contains(Path))
			{
				ExerciseClick(InEvents, Scenario.ContentTileBounds.at(Path));
			}
			break;
		case 12:
			if (++Scenario.ExerciseWait == 120)
			{
				Log(ELogLevel::Info, "Waiting for " + Path + ": " + Editor.AssetWorkspace->ActiveStatus());
				Log(ELogLevel::Info,
				    "Content selection: " + Editor.Browser->SelectedFile + "; open request: " + Editor.AssetOpenPath);
				Scenario.PlacementCapture =
				    Editor.Options.ExerciseAssets.parent_path() / ("AssetEditor-" + Name + "-Waiting.png");
				Scenario.OutlineCapture =
				    Editor.Options.ExerciseAssets.parent_path() / ("AssetWindow-" + Name + "-Waiting.png");
			}
			if (Document && PathToUtf8(Document->Loaded().Path) == Path && Editor.AssetWorkspace->IsPreviewReady())
			{
				Scenario.AssetExerciseOriginalName = ReadValue<std::string>(Document->Get("name"));
				CheckAsset(!Document->IsDirty(), "Opening an asset marked it dirty");
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetNameInput(std::vector<FInputEvent>& InEvents)
{
	const auto Name = std::string(Names.at(Scenario.AssetExerciseIndex));
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 13:
			ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("field/name"));
			break;
		case 14:
		{
			++Scenario.ExerciseWait;
			if (Scenario.ExerciseWait <= 2)
			{
				FInputEvent Key;
				Key.Type = EEventType::Key;
				Key.Key = EKey::A;
				Key.bDown = Scenario.ExerciseWait == 1;
				Key.Modifiers = Key.bDown ? 1 : 0;
				InEvents.push_back(Key);
				if (Scenario.ExerciseWait == 2)
				{
					FInputEvent Text;
					Text.Type = EEventType::Text;
					Text.Text = "Edited " + Name;
					InEvents.push_back(Text);
				}
			}
			if (Scenario.ExerciseWait == 4)
			{
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		}
		case 15:
			CheckAsset(Editor.AssetWorkspace->HasActiveInteraction(),
			           "Name input lost its active live-edit interaction");
			Scenario.PlacementCapture =
			    Editor.Options.ExerciseAssets.parent_path() / ("AssetEditor-" + Name + "-Input.png");
			++Scenario.ExerciseStep;
			break;
		case 16:
			CheckAsset(Document && Document->IsDirty() &&
			               ReadValue<std::string>(Document->Get("name")) == "Edited " + Name,
			           "Asset name input did not edit the document");
			Shortcut(InEvents, EKey::Z);
			++Scenario.ExerciseStep;
			break;
		case 17:
			CheckAsset(Document && !Document->IsDirty() &&
			               ReadValue<std::string>(Document->Get("name")) == Scenario.AssetExerciseOriginalName,
			           "Asset Undo did not restore baseline");
			Shortcut(InEvents, EKey::Y);
			++Scenario.ExerciseStep;
			break;
		case 18:
			CheckAsset(Document && Document->IsDirty(), "Asset Redo did not restore the edit");
			Shortcut(InEvents, EKey::S);
			++Scenario.ExerciseStep;
			break;
		default:
			break;
	}
}

void FEditorAcceptanceHarness::ExerciseAssetSaving(std::vector<FInputEvent>& InEvents)
{
	const auto Name = std::string(Names.at(Scenario.AssetExerciseIndex));
	const auto Path = "/Game/" + Name + ".hasset";
	const auto* Document = Editor.AssetWorkspace->ActiveDocument();
	switch (Scenario.ExerciseStep)
	{
		case 19:
			if (Document && !Document->IsSaving() && !Document->IsDirty() && Editor.AssetWorkspace->IsPreviewReady())
			{
				CheckAsset(Document->Error.empty(), "Asset save failed");
				const auto Saved = Editor.Assets.LoadAsync(Path).Get(Editor.Tasks);
				const auto Node = WriteRecord(*Saved->Type, Saved->Object.get());
				const auto& Fields = RecordFields(Node);
				CheckAsset(ReadValue<std::string>(Fields.at("name")) == "Edited " + Name,
				           "Saved asset did not retain property edit");
				CheckAsset(Editor.IsDirty() && Editor.History.size() == 1 &&
				               Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "Unsaved scene edit",
				           "Asset save discarded scene edits or history");
				Scenario.PlacementCapture =
				    Editor.Options.ExerciseAssets.parent_path() / ("AssetEditor-" + Name + ".png");
				if (Name == "Material")
				{
					Scenario.OutlineCapture = Editor.Options.ExerciseAssets.parent_path() / "AssetWindow-Scene.png";
				}
				++Scenario.ExerciseStep;
			}
			break;
		case 20:
			if (++Scenario.AssetExerciseIndex < Names.size())
			{
				Scenario.ExerciseStep = 9;
			}
			else
			{
				Scenario.AssetExerciseIndex = Names.size() - 1;
				Scenario.ExerciseStep = 100;
			}
			break;
		case 21:
			Editor.Undo();
			CheckAsset(!Editor.IsDirty(), "Scene undo baseline was lost after asset saves");
			Editor.Redo();
			CheckAsset(Editor.IsDirty() &&
			               Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "Unsaved scene edit",
			           "Scene redo was lost after asset saves");
			Editor.Window->RequestClose();
			++Scenario.ExerciseStep;
			break;
		case 22:
			ExerciseClick(InEvents, Scenario.CancelChangesBounds);
			break;
		case 23:
			CheckAsset(!Editor.Window->ShouldClose() && Editor.IsDirty(), "Cancel close lost workspace changes");
			Scenario.bAssetsVerified = true;
			Log(ELogLevel::Info, "Asset editors: five previews, native save, undo/redo, scene preservation and close "
			                     "cancellation passed");
			Editor.ResetDocument();
			Editor.Window->RequestClose();
			++Scenario.ExerciseStep;
			break;
		default:
			break;
	}
}
} // namespace Hyperion
