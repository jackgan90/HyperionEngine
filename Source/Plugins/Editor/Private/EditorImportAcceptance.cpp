#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"

namespace Hyperion
{
void FEditorPlugin::ExerciseImportInput(std::vector<FInputEvent>& InEvents)
{
	auto* Imports = Context.Find<FAssetImportWorkspace>();
	if (!Imports)
	{
		throw std::runtime_error("Import GUI acceptance needs import services");
	}
	switch (Acceptance.ExerciseStep)
	{
		case 0:
			ExerciseClick(InEvents, FileMenuBounds);
			break;
		case 1:
			ExerciseClick(InEvents, ImportMenuBounds);
			break;
		case 2:
			ExerciseClick(InEvents, ImportPanel->SourceBounds);
			break;
		case 3:
		{
			if (++Acceptance.ExerciseWait == 1)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = PathToUtf8(Options.ExerciseImport);
				InEvents.push_back(std::move(Text));
			}
			if (Acceptance.ExerciseWait > 4)
			{
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
			}
			break;
		}
		case 4:
			// Exercise the same selection handler used by the native Browse dialog.
			ImportPanel->SetOutputDirectory(Options.ExerciseImport.parent_path() / "Game/Destination");
			ExerciseClick(InEvents, ImportPanel->OutputBounds);
			break;
		case 5:
			if (!ImportPanel->DraftId.empty())
			{
				const auto Draft = Imports->Draft({ImportPanel->DraftId});
				if (Draft.Status == "failed")
				{
					throw std::runtime_error(Draft.Error);
				}
				if (Draft.Status == "ready")
				{
					if (Draft.Dimension != ETextureDimension::Texture2D || Draft.Width != 8 || Draft.Height != 8 ||
					    Draft.PixelBytes != 340 ||
					    Draft.Details != std::vector<std::string>{"Texture2D | pixel bytes: 340"})
					{
						throw std::runtime_error(
						    "GUI import texture metadata differs from the reflected draft contract");
					}
					if (ImportPanel->OutputFolder != "/Game/Destination/Color")
					{
						throw std::runtime_error("Browse selection did not update the displayed import folder");
					}
					++Acceptance.ExerciseStep;
				}
			}
			break;
		case 6:
			ExerciseClick(InEvents, ImportPanel->PreviewNameBounds);
			break;
		case 7:
		{
			++Acceptance.ExerciseWait;
			if (Acceptance.ExerciseWait <= 2)
			{
				FInputEvent Key;
				Key.Type = EEventType::Key;
				Key.Key = EKey::A;
				Key.bDown = Acceptance.ExerciseWait == 1;
				Key.Modifiers = Acceptance.ExerciseWait == 1 ? InputModifiers::Control : InputModifiers::None;
				InEvents.push_back(Key);
				if (Acceptance.ExerciseWait == 2)
				{
					FInputEvent Text;
					Text.Type = EEventType::Text;
					Text.Text = "GUI draft texture";
					InEvents.push_back(std::move(Text));
				}
			}
			if (Acceptance.ExerciseWait > 5)
			{
				Acceptance.ExerciseWait = 0;
				++Acceptance.ExerciseStep;
			}
			break;
		}
		case 8:
			ExerciseClick(InEvents, ImportPanel->UndoBounds);
			break;
		case 9:
			if (Imports->Draft({ImportPanel->DraftId}).Name != "Color")
			{
				throw std::runtime_error("Import preview undo failed");
			}
			++Acceptance.ExerciseStep;
			break;
		case 10:
			ExerciseClick(InEvents, ImportPanel->RedoBounds);
			break;
		case 11:
			if (Imports->Draft({ImportPanel->DraftId}).Name != "GUI draft texture")
			{
				throw std::runtime_error("Import preview redo failed");
			}
			++Acceptance.ExerciseStep;
			break;
		case 12:
			ExerciseClick(InEvents, ImportPanel->ImportBounds);
			break;
		case 13:
			if (ImportPanel->ImportResultBounds.Z <= ImportPanel->ImportResultBounds.X)
			{
				break;
			}
			if (!Acceptance.ExerciseWait && ImportPanel->ImportResultBounds.Z > ImportPanel->ImportResultBounds.X)
			{
				Acceptance.PlacementCapture = Options.ExerciseImport.parent_path() / "ImportSuccess.png";
			}
			ExerciseClick(InEvents, ImportPanel->ImportResultBounds);
			break;
		case 14:
		{
			const auto State = Imports->List();
			if (State.Tasks.empty() || State.Tasks.front().Status == "running")
			{
				break;
			}
			if (!ImportPanel->bOpen || State.Tasks.front().Status != "completed" ||
			    State.Tasks.front().Result->WrittenAssets != 1)
			{
				throw std::runtime_error("GUI import failed: " + State.Tasks.front().Error);
			}
			++Acceptance.ExerciseStep;
			break;
		}
		case 15:
			ExerciseClick(InEvents, FileMenuBounds);
			break;
		case 16:
			ExerciseClick(InEvents, ImportMenuBounds);
			break;
		case 17:
			ExerciseClick(InEvents, ImportPanel->ImportBounds);
			break;
		case 18:
		{
			const auto State = Imports->List();
			if (State.Tasks.size() < 2 || State.Tasks.front().Status == "running")
			{
				break;
			}
			if (!State.Tasks.front().Result || !State.Tasks.front().Result->bUpToDate ||
			    State.Tasks.front().Result->WrittenAssets != 0)
			{
				throw std::runtime_error("GUI reimport did not preserve zero-write freshness: " +
				                         State.Tasks.front().Error);
			}
			ExerciseClick(InEvents, ImportPanel->ImportResultBounds);
			break;
		}
		case 19:
		{
			if (ImportPanel->Request.Output != "/Game/Automatic.hasset")
			{
				ImportPanel->Request.Output = "/Game/Automatic.hasset";
				break;
			}
			const auto Draft = Imports->Draft({ImportPanel->DraftId});
			if (Draft.Status != "ready" || Draft.Name != "Color")
			{
				break;
			}
			FImportPropertyEdits Edits;
			Edits.Name = "Keep these edits";
			Imports->EditDraft({Draft.Draft, Draft.Generation, Edits});
			ImportPanel->Request.Output = "/Game/Changed.hasset";
			++Acceptance.ExerciseStep;
			break;
		}
		case 20:
			if (ImportPanel->RefreshCancelBounds.Z > ImportPanel->RefreshCancelBounds.X)
			{
				ExerciseClick(InEvents, ImportPanel->RefreshCancelBounds);
			}
			break;
		case 21:
			if (ImportPanel->Request.Output != "/Game/Automatic.hasset" ||
			    Imports->Draft({ImportPanel->DraftId}).Name != "Keep these edits")
			{
				throw std::runtime_error("Cancelling automatic refresh lost settings or property edits");
			}
			ImportPanel->Request.Output = "/Game/Changed.hasset";
			++Acceptance.ExerciseStep;
			break;
		case 22:
			if (ImportPanel->RefreshApplyBounds.Z > ImportPanel->RefreshApplyBounds.X)
			{
				ExerciseClick(InEvents, ImportPanel->RefreshApplyBounds);
			}
			break;
		case 23:
		{
			const auto Draft = Imports->Draft({ImportPanel->DraftId});
			if (Draft.Status != "ready" || Draft.Name != "Color")
			{
				break;
			}
			if (Draft.bDirty || ImportPanel->Request.Output != "/Game/Changed.hasset")
			{
				throw std::runtime_error("Confirmed automatic refresh did not apply the new settings");
			}
			auto Bytes = *IO.ReadAsync(Options.ExerciseImport).Get(IO.TaskSystem());
			Bytes.push_back(std::byte{});
			IO.WriteAsync(Options.ExerciseImport, std::move(Bytes)).Get(IO.TaskSystem());
			++Acceptance.ExerciseStep;
			break;
		}
		case 24:
			ExerciseClick(InEvents, ImportPanel->ImportBounds);
			break;
		case 25:
		{
			const auto State = Imports->List();
			if (State.Tasks.size() < 3 || State.Tasks.front().Status == "running")
			{
				break;
			}
			if (State.Tasks.front().Status != "failed" || State.Tasks.front().Error.empty() ||
			    IO.FileSystem()->Exists("/Game/Changed.hasset"))
			{
				throw std::runtime_error("Changed source was not rejected before publication");
			}
			if (ImportPanel->ImportResultBounds.Z <= ImportPanel->ImportResultBounds.X)
			{
				break;
			}
			if (!Acceptance.ExerciseWait && ImportPanel->ImportResultBounds.Z > ImportPanel->ImportResultBounds.X)
			{
				Acceptance.PlacementCapture = Options.ExerciseImport.parent_path() / "ImportFailure.png";
			}
			ExerciseClick(InEvents, ImportPanel->ImportResultBounds);
			break;
		}
		case 26:
		{
			const auto Draft = Imports->Draft({ImportPanel->DraftId});
			if (Draft.Status == "failed")
			{
				throw std::runtime_error("Source refresh after failed import failed: " + Draft.Error);
			}
			if (Draft.Status == "ready" && Draft.Error.empty() &&
			    ImportPanel->ImportResultBounds.Z <= ImportPanel->ImportResultBounds.X)
			{
				++Acceptance.ExerciseStep;
			}
			break;
		}
		case 27:
			ExerciseClick(InEvents, ImportPanel->ImportBounds);
			break;
		case 28:
		{
			const auto State = Imports->List();
			if (State.Tasks.size() < 4 || State.Tasks.front().Status == "running")
			{
				break;
			}
			if (State.Tasks.front().Status != "completed")
			{
				throw std::runtime_error("Retry did not use the refreshed source: " + State.Tasks.front().Error);
			}
			if (ImportPanel->OutputFolder != PathToUtf8(PathFromUtf8(State.Tasks.front().Output).parent_path()))
			{
				throw std::runtime_error("Save as does not match the current completed request");
			}
			ExerciseClick(InEvents, ImportPanel->ImportResultBounds);
			break;
		}
		case 29:
		{
			if (ImportPanel->ImportResultBounds.Z > ImportPanel->ImportResultBounds.X)
			{
				break;
			}
			auto Bytes = *IO.ReadAsync(Options.ExerciseImport).Get(IO.TaskSystem());
			Bytes.pop_back();
			IO.WriteAsync(Options.ExerciseImport, std::move(Bytes)).Get(IO.TaskSystem());
			Acceptance.PlacementCapture = Options.ExerciseImport.parent_path() / "ImportPanel.png";
			Acceptance.bImportVerified = true;
			Log(ELogLevel::Info, "Import GUI acceptance passed");
			++Acceptance.ExerciseStep;
			break;
		}
		default:
			break;
	}
}
} // namespace Hyperion
