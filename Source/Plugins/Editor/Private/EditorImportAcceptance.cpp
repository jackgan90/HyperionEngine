#include "EditorAcceptanceHarness.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
void FEditorAcceptanceHarness::ExerciseImportInput(std::vector<FInputEvent>& InEvents)
{
	auto* Imports = Editor.Context.Find<FAssetImportWorkspace>();
	if (!Imports)
	{
		throw std::runtime_error("Import GUI acceptance needs import services");
	}
	switch (Scenario.ExerciseStep)
	{
		case 0:
			ExerciseClick(InEvents, Scenario.FileMenuBounds);
			break;
		case 1:
			ExerciseClick(InEvents, Scenario.ImportMenuBounds);
			break;
		case 2:
			ExerciseClick(InEvents, Editor.ImportPanel->SourceBounds);
			break;
		case 3:
		{
			if (++Scenario.ExerciseWait == 1)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = PathToUtf8(Editor.Options.ExerciseImport);
				InEvents.push_back(std::move(Text));
			}
			if (Scenario.ExerciseWait > 4)
			{
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		}
		case 4:
			// Exercise the same selection handler used by the native Browse dialog.
			Editor.ImportPanel->SetOutputDirectory(Editor.Options.ExerciseImport.parent_path() / "Game/Destination");
			ExerciseClick(InEvents, Editor.ImportPanel->OutputBounds);
			break;
		case 5:
			if (!Editor.ImportPanel->DraftId.empty())
			{
				const auto Draft = Imports->Draft({Editor.ImportPanel->DraftId});
				if (Draft.Status == EImportDraftState::Failed)
				{
					throw std::runtime_error(Draft.Error);
				}
				if (Draft.Status == EImportDraftState::Ready)
				{
					if (Draft.Dimension != ETextureDimension::Texture2D || Draft.Width != 8 || Draft.Height != 8 ||
					    Draft.PixelBytes != 340 ||
					    Draft.Details != std::vector<std::string>{"Texture2D | pixel bytes: 340"})
					{
						throw std::runtime_error(
						    "GUI import texture metadata differs from the reflected draft contract");
					}
					if (Editor.ImportPanel->OutputFolder != "/Game/Destination/Color")
					{
						throw std::runtime_error("Browse selection did not update the displayed import folder");
					}
					++Scenario.ExerciseStep;
				}
			}
			break;
		case 6:
			ExerciseClick(InEvents, Editor.ImportPanel->PreviewNameBounds);
			break;
		case 7:
		{
			++Scenario.ExerciseWait;
			if (Scenario.ExerciseWait <= 2)
			{
				FInputEvent Key;
				Key.Type = EEventType::Key;
				Key.Key = EKey::A;
				Key.bDown = Scenario.ExerciseWait == 1;
				Key.Modifiers = Scenario.ExerciseWait == 1 ? InputModifiers::Control : InputModifiers::None;
				InEvents.push_back(Key);
				if (Scenario.ExerciseWait == 2)
				{
					FInputEvent Text;
					Text.Type = EEventType::Text;
					Text.Text = "GUI draft texture";
					InEvents.push_back(std::move(Text));
				}
			}
			if (Scenario.ExerciseWait > 5)
			{
				Scenario.ExerciseWait = 0;
				++Scenario.ExerciseStep;
			}
			break;
		}
		case 8:
			ExerciseClick(InEvents, Editor.ImportPanel->UndoBounds);
			break;
		case 9:
			if (Imports->Draft({Editor.ImportPanel->DraftId}).Name != "Color")
			{
				throw std::runtime_error("Import preview undo failed");
			}
			++Scenario.ExerciseStep;
			break;
		case 10:
			ExerciseClick(InEvents, Editor.ImportPanel->RedoBounds);
			break;
		case 11:
			if (Imports->Draft({Editor.ImportPanel->DraftId}).Name != "GUI draft texture")
			{
				throw std::runtime_error("Import preview redo failed");
			}
			++Scenario.ExerciseStep;
			break;
		case 12:
			ExerciseClick(InEvents, Editor.ImportPanel->ImportBounds);
			break;
		case 13:
			if (Editor.ImportPanel->ImportResultBounds.Z <= Editor.ImportPanel->ImportResultBounds.X)
			{
				break;
			}
			if (!Scenario.ExerciseWait &&
			    Editor.ImportPanel->ImportResultBounds.Z > Editor.ImportPanel->ImportResultBounds.X)
			{
				Scenario.PlacementCapture = Editor.Options.ExerciseImport.parent_path() / "ImportSuccess.png";
			}
			ExerciseClick(InEvents, Editor.ImportPanel->ImportResultBounds);
			break;
		case 14:
		{
			const auto State = Imports->List();
			if (State.Tasks.empty() || State.Tasks.front().Status == EImportTaskState::Running)
			{
				break;
			}
			if (!Editor.ImportPanel->bOpen || State.Tasks.front().Status != EImportTaskState::Completed ||
			    State.Tasks.front().Result->WrittenAssets != 1)
			{
				throw std::runtime_error("GUI import failed: " + State.Tasks.front().Error);
			}
			++Scenario.ExerciseStep;
			break;
		}
		case 15:
			ExerciseClick(InEvents, Scenario.FileMenuBounds);
			break;
		case 16:
			ExerciseClick(InEvents, Scenario.ImportMenuBounds);
			break;
		case 17:
			ExerciseClick(InEvents, Editor.ImportPanel->ImportBounds);
			break;
		case 18:
		{
			const auto State = Imports->List();
			if (State.Tasks.size() < 2 || State.Tasks.front().Status == EImportTaskState::Running)
			{
				break;
			}
			if (!State.Tasks.front().Result || !State.Tasks.front().Result->bUpToDate ||
			    State.Tasks.front().Result->WrittenAssets != 0)
			{
				throw std::runtime_error("GUI reimport did not preserve zero-write freshness: " +
				                         State.Tasks.front().Error);
			}
			ExerciseClick(InEvents, Editor.ImportPanel->ImportResultBounds);
			break;
		}
		case 19:
		{
			if (Editor.ImportPanel->Request.Output != "/Game/Automatic.hasset")
			{
				Editor.ImportPanel->Request.Output = "/Game/Automatic.hasset";
				break;
			}
			const auto Draft = Imports->Draft({Editor.ImportPanel->DraftId});
			if (Draft.Status != EImportDraftState::Ready || Draft.Name != "Color")
			{
				break;
			}
			FImportPropertyEdits Edits;
			Edits.Name = "Keep these edits";
			Imports->EditDraft({Draft.Draft, Draft.Generation, Edits});
			Editor.ImportPanel->Request.Output = "/Game/Changed.hasset";
			++Scenario.ExerciseStep;
			break;
		}
		case 20:
			if (Editor.ImportPanel->RefreshCancelBounds.Z > Editor.ImportPanel->RefreshCancelBounds.X)
			{
				ExerciseClick(InEvents, Editor.ImportPanel->RefreshCancelBounds);
			}
			break;
		case 21:
			if (Editor.ImportPanel->Request.Output != "/Game/Automatic.hasset" ||
			    Imports->Draft({Editor.ImportPanel->DraftId}).Name != "Keep these edits")
			{
				throw std::runtime_error("Cancelling automatic refresh lost settings or property edits");
			}
			Editor.ImportPanel->Request.Output = "/Game/Changed.hasset";
			++Scenario.ExerciseStep;
			break;
		case 22:
			if (Editor.ImportPanel->RefreshApplyBounds.Z > Editor.ImportPanel->RefreshApplyBounds.X)
			{
				ExerciseClick(InEvents, Editor.ImportPanel->RefreshApplyBounds);
			}
			break;
		case 23:
		{
			const auto Draft = Imports->Draft({Editor.ImportPanel->DraftId});
			if (Draft.Status != EImportDraftState::Ready || Draft.Name != "Color")
			{
				break;
			}
			if (Draft.bDirty || Editor.ImportPanel->Request.Output != "/Game/Changed.hasset")
			{
				throw std::runtime_error("Confirmed automatic refresh did not apply the new settings");
			}
			auto Bytes = *Editor.IO.ReadAsync(Editor.Options.ExerciseImport).Get(Editor.IO.TaskSystem());
			Bytes.push_back(std::byte{});
			Editor.IO.WriteAsync(Editor.Options.ExerciseImport, std::move(Bytes)).Get(Editor.IO.TaskSystem());
			++Scenario.ExerciseStep;
			break;
		}
		case 24:
			ExerciseClick(InEvents, Editor.ImportPanel->ImportBounds);
			break;
		case 25:
		{
			const auto State = Imports->List();
			if (State.Tasks.size() < 3 || State.Tasks.front().Status == EImportTaskState::Running)
			{
				break;
			}
			if (State.Tasks.front().Status != EImportTaskState::Failed || State.Tasks.front().Error.empty() ||
			    Editor.IO.FileSystem()->Exists("/Game/Changed.hasset"))
			{
				throw std::runtime_error("Changed source was not rejected before publication");
			}
			if (Editor.ImportPanel->ImportResultBounds.Z <= Editor.ImportPanel->ImportResultBounds.X)
			{
				break;
			}
			if (!Scenario.ExerciseWait &&
			    Editor.ImportPanel->ImportResultBounds.Z > Editor.ImportPanel->ImportResultBounds.X)
			{
				Scenario.PlacementCapture = Editor.Options.ExerciseImport.parent_path() / "ImportFailure.png";
			}
			ExerciseClick(InEvents, Editor.ImportPanel->ImportResultBounds);
			break;
		}
		case 26:
		{
			const auto Draft = Imports->Draft({Editor.ImportPanel->DraftId});
			if (Draft.Status == EImportDraftState::Failed)
			{
				throw std::runtime_error("Source refresh after failed import failed: " + Draft.Error);
			}
			if (Draft.Status == EImportDraftState::Ready && Draft.Error.empty() &&
			    Editor.ImportPanel->ImportResultBounds.Z <= Editor.ImportPanel->ImportResultBounds.X)
			{
				++Scenario.ExerciseStep;
			}
			break;
		}
		case 27:
			ExerciseClick(InEvents, Editor.ImportPanel->ImportBounds);
			break;
		case 28:
		{
			const auto State = Imports->List();
			if (State.Tasks.size() < 4 || State.Tasks.front().Status == EImportTaskState::Running)
			{
				break;
			}
			if (State.Tasks.front().Status != EImportTaskState::Completed)
			{
				throw std::runtime_error("Retry did not use the refreshed source: " + State.Tasks.front().Error);
			}
			if (Editor.ImportPanel->OutputFolder != PathToUtf8(PathFromUtf8(State.Tasks.front().Output).parent_path()))
			{
				throw std::runtime_error("Save as does not match the current completed request");
			}
			ExerciseClick(InEvents, Editor.ImportPanel->ImportResultBounds);
			break;
		}
		case 29:
		{
			if (Editor.ImportPanel->ImportResultBounds.Z > Editor.ImportPanel->ImportResultBounds.X)
			{
				break;
			}
			auto Bytes = *Editor.IO.ReadAsync(Editor.Options.ExerciseImport).Get(Editor.IO.TaskSystem());
			Bytes.pop_back();
			Editor.IO.WriteAsync(Editor.Options.ExerciseImport, std::move(Bytes)).Get(Editor.IO.TaskSystem());
			Scenario.PlacementCapture = Editor.Options.ExerciseImport.parent_path() / "ImportPanel.png";
			Scenario.bImportVerified = true;
			Log(ELogLevel::Info, "Import GUI acceptance passed");
			++Scenario.ExerciseStep;
			break;
		}
		default:
			break;
	}
}
} // namespace Hyperion
