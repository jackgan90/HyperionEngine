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
	const auto Stage = DescribeAssetAcceptance(Scenario.Asset.Progress.GetState()).Stage;
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
	if ((Stage == EAssetAcceptanceStage::WindowFixture || Stage == EAssetAcceptanceStage::WindowHistory ||
	     Stage == EAssetAcceptanceStage::WindowSizing || Stage == EAssetAcceptanceStage::WindowClosing ||
	     Stage == EAssetAcceptanceStage::WindowSaving))
	{
		ExerciseAssetWindowInput(InEvents);
		return;
	}
	if ((Stage == EAssetAcceptanceStage::Material || Stage == EAssetAcceptanceStage::References ||
	     Stage == EAssetAcceptanceStage::Texture || Stage == EAssetAcceptanceStage::CustomMaterial ||
	     Stage == EAssetAcceptanceStage::CustomMaterialReset || Stage == EAssetAcceptanceStage::CleanTab ||
	     Stage == EAssetAcceptanceStage::DiscardTab || Stage == EAssetAcceptanceStage::FloatingPanel))
	{
		ExerciseAssetPropertyInput(InEvents);
		return;
	}
	const auto Name = std::string(Names.at(Scenario.AssetExerciseIndex));
	if (Scenario.AssetLoggedState != Scenario.Asset.Progress.GetState())
	{
		Scenario.AssetLoggedState = Scenario.Asset.Progress.GetState();
		Log(ELogLevel::Info, "Asset acceptance " + Name + " step " + Scenario.Asset.Progress.Name());
	}
	if ((Stage == EAssetAcceptanceStage::Opening))
	{
		ExerciseAssetOpening(InEvents);
	}
	else if ((Stage == EAssetAcceptanceStage::Name))
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
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::SelectContentRoot:
			Editor.QueueContentRoot(Editor.Options.ExerciseAssets);
			Scenario.Asset.Progress.TransitionTo(EAssetState::OpenScene);
			break;
		case EAssetState::OpenScene:
			Editor.RequestOpenAsset("/Game/Scene.hasset");
			Scenario.Asset.Progress.TransitionTo(EAssetState::PrepareDirtyScene);
			break;
		case EAssetState::PrepareDirtyScene:
			if (Editor.Scene->GetStatus().bReady && Editor.ReadyFrames > 8)
			{
				const auto Handle = Editor.Scene->FindHandle("model");
				auto Node = *Editor.Scene->FindNode(Handle);
				Node.Name = "Unsaved scene edit";
				Node.Model()->Material.Roughness = .73f;
				Editor.CommitEdit(Handle, Node, Editor.Scene->GetRevision());
				Editor.Selection = Handle;
				Scenario.Asset.Progress.TransitionTo(EAssetState::RevealAsset);
			}
			break;
		case EAssetState::RevealAsset:
			Scenario.ContentRevealPath = Path;
			Scenario.Asset.Progress.TransitionTo(EAssetState::SelectAsset);
			break;
		case EAssetState::SelectAsset:
		case EAssetState::DoubleClickAsset:
			if (Scenario.ContentTileBounds.contains(Path))
			{
				if (ExerciseClick(InEvents, Scenario.ContentTileBounds.at(Path), Scenario.Asset.Click))
				{
					Scenario.Asset.Progress.TransitionTo(Scenario.Asset.Progress.Is(EAssetState::SelectAsset)
					                                         ? EAssetState::DoubleClickAsset
					                                         : EAssetState::AwaitAssetPreview);
				}
			}
			break;
		case EAssetState::AwaitAssetPreview:
			Scenario.Asset.PreviewObservation.Advance();
			if (Scenario.Asset.PreviewObservation.IsAt(FAssetScenarioContext::PreviewDiagnosticFrames))
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
				Scenario.Asset.PreviewObservation.Restart();
				Scenario.Asset.Progress.TransitionTo(EAssetState::FocusName);
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
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::FocusName:
			if (ExerciseClick(InEvents, Editor.AssetWorkspace->ObservedBounds("field/name"), Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::TypeName);
			}
			break;
		case EAssetState::TypeName:
			if (ExerciseTextInput(InEvents, Scenario.Asset.NameInput, "Edited " + Name))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::CaptureNameInteraction);
			}
			break;
		case EAssetState::CaptureNameInteraction:
			CheckAsset(Editor.AssetWorkspace->HasActiveInteraction(),
			           "Name input lost its active live-edit interaction");
			Scenario.PlacementCapture =
			    Editor.Options.ExerciseAssets.parent_path() / ("AssetEditor-" + Name + "-Input.png");
			Scenario.Asset.Progress.TransitionTo(EAssetState::UndoName);
			break;
		case EAssetState::UndoName:
			CheckAsset(Document && Document->IsDirty() &&
			               ReadValue<std::string>(Document->Get("name")) == "Edited " + Name,
			           "Asset name input did not edit the document");
			Shortcut(InEvents, EKey::Z);
			Scenario.Asset.Progress.TransitionTo(EAssetState::RedoName);
			break;
		case EAssetState::RedoName:
			CheckAsset(Document && !Document->IsDirty() &&
			               ReadValue<std::string>(Document->Get("name")) == Scenario.AssetExerciseOriginalName,
			           "Asset Undo did not restore baseline");
			Shortcut(InEvents, EKey::Y);
			Scenario.Asset.Progress.TransitionTo(EAssetState::SaveName);
			break;
		case EAssetState::SaveName:
			CheckAsset(Document && Document->IsDirty(), "Asset Redo did not restore the edit");
			Shortcut(InEvents, EKey::S);
			Scenario.Asset.Progress.TransitionTo(EAssetState::AwaitSavedName);
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
	switch (Scenario.Asset.Progress.GetState())
	{
		case EAssetState::AwaitSavedName:
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
				Scenario.Asset.Progress.TransitionTo(EAssetState::AdvanceAsset);
			}
			break;
		case EAssetState::AdvanceAsset:
			if (++Scenario.AssetExerciseIndex < Names.size())
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::RevealAsset);
			}
			else
			{
				Scenario.AssetExerciseIndex = Names.size() - 1;
				Scenario.Asset.Progress.TransitionTo(EAssetState::OpenMaterial);
			}
			break;
		case EAssetState::VerifySceneHistoryAndRequestClose:
			Editor.Undo();
			CheckAsset(!Editor.IsDirty(), "Scene undo baseline was lost after asset saves");
			Editor.Redo();
			CheckAsset(Editor.IsDirty() &&
			               Editor.Scene->FindNode(Editor.Scene->FindHandle("model"))->Name == "Unsaved scene edit",
			           "Scene redo was lost after asset saves");
			Editor.Window->RequestClose();
			Scenario.Asset.Progress.TransitionTo(EAssetState::CancelMainClose);
			break;
		case EAssetState::CancelMainClose:
			if (ExerciseClick(InEvents, Scenario.CancelChangesBounds, Scenario.Asset.Click))
			{
				Scenario.Asset.Progress.TransitionTo(EAssetState::VerifyMainCloseCancellation);
			}
			break;
		case EAssetState::VerifyMainCloseCancellation:
			CheckAsset(!Editor.Window->ShouldClose() && Editor.IsDirty(), "Cancel close lost workspace changes");
			Scenario.bAssetsVerified = true;
			Log(ELogLevel::Info, "Asset editors: five previews, native save, undo/redo, scene preservation and close "
			                     "cancellation passed");
			Editor.ResetDocument();
			Editor.Window->RequestClose();
			Scenario.Asset.Progress.TransitionTo(EAssetState::Complete);
			break;
		default:
			break;
	}
}
} // namespace Hyperion
