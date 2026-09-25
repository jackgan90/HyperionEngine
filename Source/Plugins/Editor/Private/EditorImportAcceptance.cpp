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
	switch (ExerciseStep)
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
			if (++ExerciseWait == 1)
			{
				FInputEvent Text;
				Text.Type = EEventType::Text;
				Text.Text = PathToUtf8(Options.ExerciseImport);
				InEvents.push_back(std::move(Text));
			}
			if (ExerciseWait > 4)
			{
				ExerciseWait = 0;
				++ExerciseStep;
			}
			break;
		}
		case 4:
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
					++ExerciseStep;
				}
			}
			break;
		case 6:
			ExerciseClick(InEvents, ImportPanel->PreviewNameBounds);
			break;
		case 7:
		{
			++ExerciseWait;
			if (ExerciseWait <= 2)
			{
				FInputEvent Key;
				Key.Type = EEventType::Key;
				Key.Key = EKey::A;
				Key.bDown = ExerciseWait == 1;
				Key.Modifiers = ExerciseWait == 1 ? 1 : 0;
				InEvents.push_back(Key);
				if (ExerciseWait == 2)
				{
					FInputEvent Text;
					Text.Type = EEventType::Text;
					Text.Text = "GUI draft texture";
					InEvents.push_back(std::move(Text));
				}
			}
			if (ExerciseWait > 5)
			{
				ExerciseWait = 0;
				++ExerciseStep;
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
			++ExerciseStep;
			break;
		case 10:
			ExerciseClick(InEvents, ImportPanel->RedoBounds);
			break;
		case 11:
			if (Imports->Draft({ImportPanel->DraftId}).Name != "GUI draft texture")
			{
				throw std::runtime_error("Import preview redo failed");
			}
			++ExerciseStep;
			break;
		case 12:
			ExerciseClick(InEvents, ImportPanel->ImportBounds);
			break;
		case 13:
			ExerciseClick(InEvents, ImportPanel->CloseBounds);
			break;
		case 14:
		{
			const auto State = Imports->List();
			if (State.Tasks.empty() || State.Tasks.front().Status == "running")
			{
				break;
			}
			if (ImportPanel->bOpen || State.Tasks.front().Status != "completed" ||
			    State.Tasks.front().Result->WrittenAssets != 1)
			{
				throw std::runtime_error("GUI import failed: " + State.Tasks.front().Error);
			}
			++ExerciseStep;
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
			PlacementCapture = Options.ExerciseImport.parent_path() / "ImportPanel.png";
			bImportVerified = true;
			Log(ELogLevel::Info, "Import GUI acceptance passed");
			++ExerciseStep;
			break;
		}
		default:
			break;
	}
}
} // namespace Hyperion
