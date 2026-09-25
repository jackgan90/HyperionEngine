#include "AssetImportPanel.h"
#include "Hyperion/Reflection/Wire.h"
#include <algorithm>

namespace Hyperion
{
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
		const bool bAutomatic = bAutoPrepare && !bSourceEditing && !PreviewInfo.bDirty;
		if (!bRefreshDraft && !bAutomatic)
		{
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
		PreviewOffset = 0;
		SelectedNode = SelectedPrimitive = 0;
		bPreviewOpen = bPreviewFocus = true;
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

void FAssetImportPanel::DrawPreview(FGui& InGui)
{
	if (!bPreviewOpen)
	{
		return;
	}
	if (InGui.BeginWindow("Import Property Preview", bPreviewOpen, {620, 680}, {740, 40}))
	{
		if (std::exchange(bPreviewFocus, false))
		{
			InGui.FocusWindow("Import Property Preview");
		}
		InGui.TextWrapped("Unpublished root properties. Generated dependencies and scene structure are read-only.");
		InGui.TextWrapped("Status: " + (DraftId.empty() ? std::string("no draft") : PreviewInfo.Status));
		if (PreviewInfo.bDirty)
		{
			InGui.TextWrapped("Unpublished property changes");
		}
		if (!PreviewInfo.Error.empty())
		{
			InGui.TextWrapped(PreviewInfo.Error);
		}
		const bool bStale = PreparedKey != RequestKey();
		if (bStale)
		{
			InGui.TextWrapped(
			    "Source/settings changed. Update preview in Import Asset; existing property edits will be discarded.");
		}
		InGui.BeginDisabled(DraftId.empty() || PreviewInfo.Status != "ready" || bStale);
		try
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
			DrawDraftProperties(InGui, PreviewInfo);
		}
		catch (const std::exception& Failure)
		{
			Message = Failure.what();
		}
		InGui.EndDisabled();
		InGui.Separator();
		InGui.BeginDisabled(DraftId.empty() || PreviewInfo.Status == "preparing" || PreviewInfo.Status == "publishing");
		if (InGui.Button("Discard draft and changes"))
		{
			Imports->DiscardDraft({DraftId, PreviewInfo.Generation, true});
			DraftId.clear();
			PreviewInfo = {};
			bPreviewOpen = false;
		}
		InGui.EndDisabled();
		InGui.TextWrapped("Close hides this window. Use Import in the parent panel to publish; no files are written by "
		                  "property edits.");
		if (!Message.empty())
		{
			InGui.TextWrapped(Message);
		}
	}
	InGui.EndWindow();
}
} // namespace Hyperion
