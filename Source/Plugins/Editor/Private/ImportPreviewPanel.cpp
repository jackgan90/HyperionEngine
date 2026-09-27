#include "AssetImportPanel.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Reflection/Wire.h"
#include <algorithm>

namespace Hyperion
{
void FAssetImportPanel::ShowImportResult(std::string InMessage)
{
	ImportTask.clear();
	Message.clear();
	ImportResultMessage = std::move(InMessage);
	bImportResultPending = bImportResultOpen = true;
}

void FAssetImportPanel::SubmitImport()
{
	try
	{
		ImportTask = Imports->SubmitDraft({DraftId, PreviewInfo.Generation}).Task;
		Message.clear();
	}
	catch (const std::exception& Failure)
	{
		ShowImportResult("Import failed.\n" + std::string(Failure.what()));
	}
}

void FAssetImportPanel::ProcessImportResult()
{
	if (!Imports || ImportTask.empty())
	{
		return;
	}
	try
	{
		const auto Task = Imports->Get({ImportTask});
		if (Task.Status == "running")
		{
			return;
		}
		if (Task.Status == "completed")
		{
			ValidatedOutputKey.clear();
			const auto Warning = Task.Result ? Task.Result->Warning : std::string{};
			ShowImportResult(Warning.empty() ? "Import completed successfully.\n" + Task.Output
			                                 : "Import completed.\n" + Task.Output + "\n" + Warning);
		}
		else
		{
			// Retry preparation once after the result is dismissed; never resubmit automatically.
			bAutoPrepare = true;
			ValidatedOutputKey.clear();
			ShowImportResult("Import failed.\n" + Task.Error);
		}
	}
	catch (const std::exception& Failure)
	{
		ShowImportResult("Import failed.\n" + std::string(Failure.what()));
	}
}

void FAssetImportPanel::DrawImportResult(FGui& InGui)
{
	ImportResultBounds = {};
	if (std::exchange(bImportResultPending, false))
	{
		InGui.OpenPopup("Import result");
	}
	if (InGui.BeginMessageModal("Import result", bImportResultOpen, ImportResultMessage))
	{
		if (InGui.CenteredButton("OK"))
		{
			bImportResultOpen = false;
			InGui.ClosePopup();
		}
		ImportResultBounds = InGui.LastItemBounds();
		InGui.EndModal();
	}
}

std::string FAssetImportPanel::RequestKey() const
{
	const auto Value = Snapshot();
	return HashArchive(WriteRecord(RecordType<FImportRequest>(), &Value));
}

void FAssetImportPanel::ProcessDraft()
{
	if (!Imports)
	{
		return;
	}
	try
	{
		if (!DraftId.empty())
		{
			try
			{
				PreviewInfo = Imports->Draft({DraftId, PreviewOffset});
			}
			catch (const FAssetImportError& Failure)
			{
				if (Failure.Code != "not_found")
				{
					throw;
				}
				DraftId.clear();
				PreviewInfo = {};
			}
			if (PreviewInfo.Status == "preparing" || PreviewInfo.Status == "publishing")
			{
				return;
			}
		}
		const auto Key = RequestKey();
		if (ObservedRequestKey != Key)
		{
			ObservedRequestKey = Key;
			bAutoPrepare = true;
		}
		if (!bAutoPrepare || bSourceEditing || bSettingsEditing || bImportResultOpen)
		{
			return;
		}
		if (PreviewInfo.bDirty && !bRefreshDraft)
		{
			if (!bRefreshDialog)
			{
				bConfirmRefresh = bRefreshDialog = true;
			}
			return;
		}
		bRefreshDraft = bAutoPrepare = false;
		if (Request.Source.empty())
		{
			return;
		}
		// Validate before releasing the old draft; a bad path does not lose editable work.
		(void)Imports->Validate(Snapshot());
		if (!DraftId.empty())
		{
			Imports->DiscardDraft({DraftId, PreviewInfo.Generation, true});
			DraftId.clear();
		}
		PreviewInfo = Imports->PrepareDraft(Snapshot());
		DraftId = PreviewInfo.Draft;
		PreparedKey = RequestKey();
		PreparedRequest = Snapshot();
		PreviewOffset = 0;
		SelectedNode = SelectedPrimitive = 0;
		Message.clear();
	}
	catch (const FAssetImportError& Failure)
	{
		Message = Failure.what();
	}
	catch (const std::exception& Failure)
	{
		Message = Failure.what();
	}
}

void FAssetImportPanel::ApplyDraftEdit(const FImportDraftInfo& InInfo, FImportPropertyEdits InEdits)
{
	Imports->EditDraft({DraftId, InInfo.Generation, std::move(InEdits)});
	PreviewInfo = Imports->Draft({DraftId, PreviewOffset});
}

void FAssetImportPanel::DrawImportProperties(FGui& InGui)
{
	if (!InGui.Section("Asset properties"))
	{
		return;
	}
	InGui.Indent();
	if (DraftId.empty())
	{
		InGui.Text("Select a source file to load its properties.");
	}
	else if (PreviewInfo.Status == "preparing")
	{
		InGui.Text("Loading properties...");
	}
	else
	{
		const bool bStale = PreparedKey != RequestKey();
		if (bStale)
		{
			InGui.TextWrapped("Updating properties for the selected source and settings...");
		}
		InGui.BeginDisabled(PreviewInfo.Status != "ready" || bStale);
		try
		{
			DrawDraftProperties(InGui, PreviewInfo);
			DrawPropertyHistory(InGui);
		}
		catch (const std::exception& Failure)
		{
			Message = Failure.what();
		}
		InGui.EndDisabled();
	}
	InGui.Unindent();
}

void FAssetImportPanel::DrawPropertyHistory(FGui& InGui)
{
	if (InGui.Button("Undo", PreviewInfo.bCanUndo))
	{
		Imports->DraftHistory({DraftId, PreviewInfo.Generation, "undo"});
		PreviewInfo = Imports->Draft({DraftId, PreviewOffset});
	}
	UndoBounds = InGui.LastItemBounds();
	InGui.SameLine();
	if (InGui.Button("Redo", PreviewInfo.bCanRedo))
	{
		Imports->DraftHistory({DraftId, PreviewInfo.Generation, "redo"});
		PreviewInfo = Imports->Draft({DraftId, PreviewOffset});
	}
	RedoBounds = InGui.LastItemBounds();
	InGui.SameLine();
	if (InGui.Button("Reset to source"))
	{
		Imports->DraftHistory({DraftId, PreviewInfo.Generation, "reset"});
		PreviewInfo = Imports->Draft({DraftId, PreviewOffset});
	}
}

void FAssetImportPanel::RestorePreparedRequest()
{
	Request = PreparedRequest;
	PreviousSource = Request.Source;
	const auto Directory = PathFromUtf8(Request.Output).parent_path().lexically_relative("/Game");
	Preferences.ImportOutputDirectory = (PathFromUtf8(Roots.Info().Directory) / Directory).lexically_normal();
	SavePreferences();
	ValidatedOutputKey.clear();
	const auto Formats = FAssetImportWorkspace::Capabilities().Formats;
	TypeIndex = 0;
	for (std::size_t Index = 0; Index < Formats.size(); ++Index)
	{
		if (Formats[Index].Type == Request.Type)
		{
			TypeIndex = Index + 1;
		}
	}
	if (Request.TextureEncoding)
	{
		EncodingIndex = static_cast<std::size_t>(*Request.TextureEncoding);
	}
	if (Request.Sky)
	{
		Bake = *Request.Sky;
	}
	ObservedRequestKey = PreparedKey;
	bAutoPrepare = bRefreshDraft = bRefreshDialog = false;
}

void FAssetImportPanel::DrawRefreshConfirmation(FGui& InGui)
{
	RefreshApplyBounds = RefreshCancelBounds = {};
	if (std::exchange(bConfirmRefresh, false))
	{
		InGui.OpenPopup("Reset property changes?");
	}
	const bool bWasOpen = bRefreshDialog;
	if (InGui.BeginModal("Reset property changes?", bRefreshDialog))
	{
		InGui.TextWrapped("Changing the source or import settings will reset your property edits.");
		if (InGui.Button("Apply and reset"))
		{
			bRefreshDraft = bAutoPrepare = true;
			bRefreshDialog = false;
			InGui.ClosePopup();
		}
		RefreshApplyBounds = InGui.LastItemBounds();
		InGui.SameLine();
		if (InGui.Button("Keep current settings"))
		{
			RestorePreparedRequest();
			InGui.ClosePopup();
		}
		RefreshCancelBounds = InGui.LastItemBounds();
		InGui.EndModal();
	}
	else if (bWasOpen && !bRefreshDialog && PreviewInfo.bDirty && !bRefreshDraft)
	{
		RestorePreparedRequest();
	}
}
} // namespace Hyperion
