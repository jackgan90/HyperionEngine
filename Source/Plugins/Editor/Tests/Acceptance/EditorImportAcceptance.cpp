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
	switch (Scenario.Import.Progress.GetState())
	{
		case EImportState::OpenFileMenu:
			if (ExerciseClick(InEvents, Scenario.FileMenuBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::OpenImportMenu);
			}
			break;
		case EImportState::OpenImportMenu:
			if (ExerciseClick(InEvents, Scenario.ImportMenuBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::FocusSource);
			}
			break;
		case EImportState::FocusSource:
			if (ExerciseClick(InEvents, Editor.ImportPanel->SourceBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::TypeSource);
			}
			break;
		case EImportState::TypeSource:
			if (Scenario.Import.SourceEntry.Is(ESourceTextEntryPhase::EnterText))
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = PathToUtf8(Editor.Options.ExerciseImport);
				InEvents.push_back(std::move(Text));
				Scenario.Import.SourceEntry.TransitionTo(ESourceTextEntryPhase::AwaitTextCommit);
			}
			else if (Scenario.Import.SourceTextCommit.Advance())
			{
				Scenario.Import.Progress.TransitionTo(EImportState::SelectOutputDirectory);
			}
			break;
		case EImportState::SelectOutputDirectory:
			// Exercise the same selection handler used by the native Browse dialog.
			Editor.ImportPanel->SetOutputDirectory(Editor.Options.ExerciseImport.parent_path() / "Game/Destination");
			if (ExerciseClick(InEvents, Editor.ImportPanel->OutputBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::AwaitDraftPreview);
			}
			break;
		case EImportState::AwaitDraftPreview:
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
					Scenario.Import.Progress.TransitionTo(EImportState::FocusDraftName);
				}
			}
			break;
		case EImportState::FocusDraftName:
			if (ExerciseClick(InEvents, Editor.ImportPanel->PreviewNameBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::TypeDraftName);
			}
			break;
		case EImportState::TypeDraftName:
			if (ExerciseTextInput(InEvents, Scenario.Import.DraftName, "GUI draft texture"))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::UndoDraftName);
			}
			break;
		case EImportState::UndoDraftName:
			if (ExerciseClick(InEvents, Editor.ImportPanel->UndoBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::VerifyDraftUndo);
			}
			break;
		case EImportState::VerifyDraftUndo:
			if (Imports->Draft({Editor.ImportPanel->DraftId}).Name != "Color")
			{
				throw std::runtime_error("Import preview undo failed");
			}
			Scenario.Import.Progress.TransitionTo(EImportState::RedoDraftName);
			break;
		case EImportState::RedoDraftName:
			if (ExerciseClick(InEvents, Editor.ImportPanel->RedoBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::VerifyDraftRedo);
			}
			break;
		case EImportState::VerifyDraftRedo:
			if (Imports->Draft({Editor.ImportPanel->DraftId}).Name != "GUI draft texture")
			{
				throw std::runtime_error("Import preview redo failed");
			}
			Scenario.Import.Progress.TransitionTo(EImportState::StartImport);
			break;
		case EImportState::StartImport:
			if (ExerciseClick(InEvents, Editor.ImportPanel->ImportBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::OpenImportResult);
			}
			break;
		case EImportState::OpenImportResult:
			if (Editor.ImportPanel->ImportResultBounds.Z <= Editor.ImportPanel->ImportResultBounds.X)
			{
				break;
			}
			if (!Scenario.Import.bImportResultCaptured &&
			    Editor.ImportPanel->ImportResultBounds.Z > Editor.ImportPanel->ImportResultBounds.X)
			{
				Scenario.PlacementCapture = Editor.Options.ExerciseImport.parent_path() / "ImportSuccess.png";
				Scenario.Import.bImportResultCaptured = true;
			}
			if (ExerciseClick(InEvents, Editor.ImportPanel->ImportResultBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::AwaitImportResult);
			}
			break;
		case EImportState::AwaitImportResult:
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
			Scenario.Import.Progress.TransitionTo(EImportState::ReopenFileMenu);
			break;
		}
		case EImportState::ReopenFileMenu:
			if (ExerciseClick(InEvents, Scenario.FileMenuBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::ReopenImportMenu);
			}
			break;
		case EImportState::ReopenImportMenu:
			if (ExerciseClick(InEvents, Scenario.ImportMenuBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::StartReimport);
			}
			break;
		case EImportState::StartReimport:
			if (ExerciseClick(InEvents, Editor.ImportPanel->ImportBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::VerifyReimportResult);
			}
			break;
		case EImportState::VerifyReimportResult:
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
			if (ExerciseClick(InEvents, Editor.ImportPanel->ImportResultBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::PrepareCancelledRefresh);
			}
			break;
		}
		case EImportState::PrepareCancelledRefresh:
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
			Scenario.Import.Progress.TransitionTo(EImportState::CancelRefresh);
			break;
		}
		case EImportState::CancelRefresh:
			if (Editor.ImportPanel->RefreshCancelBounds.Z > Editor.ImportPanel->RefreshCancelBounds.X)
			{
				if (ExerciseClick(InEvents, Editor.ImportPanel->RefreshCancelBounds, Scenario.Import.Click))
				{
					Scenario.Import.Progress.TransitionTo(EImportState::VerifyCancelledRefresh);
				}
			}
			break;
		case EImportState::VerifyCancelledRefresh:
			if (Editor.ImportPanel->Request.Output != "/Game/Automatic.hasset" ||
			    Imports->Draft({Editor.ImportPanel->DraftId}).Name != "Keep these edits")
			{
				throw std::runtime_error("Cancelling automatic refresh lost settings or property edits");
			}
			Editor.ImportPanel->Request.Output = "/Game/Changed.hasset";
			Scenario.Import.Progress.TransitionTo(EImportState::ApplyRefresh);
			break;
		case EImportState::ApplyRefresh:
			if (Editor.ImportPanel->RefreshApplyBounds.Z > Editor.ImportPanel->RefreshApplyBounds.X)
			{
				if (ExerciseClick(InEvents, Editor.ImportPanel->RefreshApplyBounds, Scenario.Import.Click))
				{
					Scenario.Import.Progress.TransitionTo(EImportState::VerifyRefreshAndChangeSource);
				}
			}
			break;
		case EImportState::VerifyRefreshAndChangeSource:
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
			Scenario.Import.Progress.TransitionTo(EImportState::ImportChangedSource);
			break;
		}
		case EImportState::ImportChangedSource:
			if (ExerciseClick(InEvents, Editor.ImportPanel->ImportBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::VerifyChangedSourceRejection);
			}
			break;
		case EImportState::VerifyChangedSourceRejection:
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
			if (!Scenario.Import.bRefreshResultCaptured &&
			    Editor.ImportPanel->ImportResultBounds.Z > Editor.ImportPanel->ImportResultBounds.X)
			{
				Scenario.PlacementCapture = Editor.Options.ExerciseImport.parent_path() / "ImportFailure.png";
				Scenario.Import.bRefreshResultCaptured = true;
			}
			if (ExerciseClick(InEvents, Editor.ImportPanel->ImportResultBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::AwaitRefreshedSource);
			}
			break;
		}
		case EImportState::AwaitRefreshedSource:
		{
			const auto Draft = Imports->Draft({Editor.ImportPanel->DraftId});
			if (Draft.Status == EImportDraftState::Failed)
			{
				throw std::runtime_error("Source refresh after failed import failed: " + Draft.Error);
			}
			if (Draft.Status == EImportDraftState::Ready && Draft.Error.empty() &&
			    Editor.ImportPanel->ImportResultBounds.Z <= Editor.ImportPanel->ImportResultBounds.X)
			{
				Scenario.Import.Progress.TransitionTo(EImportState::RetryImport);
			}
			break;
		}
		case EImportState::RetryImport:
			if (ExerciseClick(InEvents, Editor.ImportPanel->ImportBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::VerifyRetryResult);
			}
			break;
		case EImportState::VerifyRetryResult:
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
			if (ExerciseClick(InEvents, Editor.ImportPanel->ImportResultBounds, Scenario.Import.Click))
			{
				Scenario.Import.Progress.TransitionTo(EImportState::RestoreSourceAndComplete);
			}
			break;
		}
		case EImportState::RestoreSourceAndComplete:
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
			Scenario.Import.Progress.TransitionTo(EImportState::Complete);
			break;
		}
		default:
			break;
	}
}
} // namespace Hyperion
