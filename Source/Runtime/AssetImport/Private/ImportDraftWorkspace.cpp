#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/IO/Path.h"
#include <algorithm>

namespace Hyperion
{
std::shared_ptr<FImportDraft> FAssetImportWorkspace::FindDraft(const std::string& InId) const
{
	RequireMain();
	for (const auto& Entry : Drafts)
	{
		if (Entry->Id == InId)
		{
			return Entry;
		}
	}
	throw FAssetImportError("not_found", "Import draft was discarded or belongs to another root");
}

std::shared_ptr<FImportDraft> FAssetImportWorkspace::MutableDraft(const std::string& InId,
                                                                  std::uint64_t InGeneration) const
{
	auto Entry = FindDraft(InId);
	if (bClosing || Entry->Status == "preparing" || Entry->Status == "publishing")
	{
		throw FAssetImportError("busy", "Wait for import draft work to finish");
	}
	if (Entry->Generation != InGeneration)
	{
		throw FAssetImportError("stale_revision", "Import draft changed before the operation");
	}
	return Entry;
}

FImportDraftList FAssetImportWorkspace::DraftList() const
{
	RequireMain();
	FImportDraftList Result;
	for (const auto& Entry : Drafts)
	{
		Result.Drafts.push_back(Entry->Id);
	}
	return Result;
}

FImportDraftInfo FAssetImportWorkspace::PrepareDraft(const FImportRequest& InRequest)
{
	RequireMain();
	if (bClosing || Drafts.size() >= 4)
	{
		throw FAssetImportError("busy", "Discard an import draft before preparing more (maximum four)");
	}
	const auto Validated = Validate(InRequest);
	auto Entry = std::make_shared<FImportDraft>();
	Entry->Id = CreateIdentifier();
	Entry->Request = InRequest;
	Drafts.push_back(Entry);
	try
	{
		Entry->Pending =
		    Imports.PrepareAsync(PathFromUtf8(Validated.Source), PathFromUtf8(Validated.Output), Options(InRequest));
	}
	catch (...)
	{
		Drafts.pop_back();
		throw;
	}
	return Draft({Entry->Id});
}

void FAssetImportWorkspace::UpdateDrafts()
{
	for (const auto& Entry : Drafts)
	{
		if (Entry->Status == "publishing")
		{
			const auto& Task = Entry->Task->Info;
			if (Task.Status != "running")
			{
				Entry->Status = "ready";
				Entry->Error = Task.Error;
				if (Task.Status == "completed")
				{
					Entry->SavedKey = ImportPropertiesKey(Entry->History[Entry->Cursor]);
				}
				++Entry->Generation;
			}
		}
		if (Entry->Status != "preparing" || !Entry->Pending.Ready())
		{
			continue;
		}
		try
		{
			auto Candidate = *Entry;
			Candidate.Prepared = Entry->Pending.GetReady();
			Candidate.Edited = Candidate.Prepared->Root;
			Candidate.Status = "ready";
			Candidate.Pending = {};
			(void)CommitDraft(Entry, std::move(Candidate));
		}
		catch (const std::exception& Failure)
		{
			Entry->Status = "failed";
			Entry->Error = std::string(Failure.what()).substr(0, 8192);
			++Entry->Generation;
			Log(ELogLevel::Error, "Import draft preparation failed; draft='" + Entry->Id + "'; source='" +
			                          Entry->Request.Source + "'; output='" + Entry->Request.Output +
			                          "'; reason=" + Failure.what());
		}
		Entry->Pending = {};
	}
}

FImportDraftInfo FAssetImportWorkspace::EditDraft(const FImportDraftEdit& InRequest)
{
	auto Entry = MutableDraft(InRequest.Draft, InRequest.Generation);
	if (!Entry->Prepared)
	{
		throw FAssetImportError("unavailable", "Draft preparation failed; refresh it");
	}
	const auto Key = ImportPropertiesKey(InRequest.Properties);
	if (Key == ImportPropertiesKey(Entry->History[Entry->Cursor]))
	{
		return Draft({Entry->Id});
	}
	auto Edited = ApplyImportProperties(Entry->Prepared->Root, InRequest.Properties);
	auto Candidate = *Entry;
	Candidate.History.resize(Candidate.Cursor + 1);
	Candidate.History.push_back(InRequest.Properties);
	if (Candidate.History.size() > 65)
	{
		Candidate.History.erase(Candidate.History.begin());
	}
	Candidate.Cursor = Candidate.History.size() - 1;
	Candidate.Edited = std::move(Edited);
	Candidate.Error.clear();
	return CommitDraft(Entry, std::move(Candidate));
}

FImportDraftInfo FAssetImportWorkspace::DraftHistory(const FImportDraftHistory& InRequest)
{
	auto Entry = MutableDraft(InRequest.Draft, InRequest.Generation);
	if (InRequest.Action == "reset")
	{
		return EditDraft({Entry->Id, Entry->Generation, {}});
	}
	auto Cursor = Entry->Cursor;
	if (InRequest.Action == "undo" && Cursor)
	{
		--Cursor;
	}
	else if (InRequest.Action == "redo" && Cursor + 1 < Entry->History.size())
	{
		++Cursor;
	}
	else
	{
		throw FAssetImportError("invalid_arguments", "History requires available undo, redo or reset");
	}
	auto Edited = ApplyImportProperties(Entry->Prepared->Root, Entry->History[Cursor]);
	auto Candidate = *Entry;
	Candidate.Edited = std::move(Edited);
	Candidate.Cursor = Cursor;
	return CommitDraft(Entry, std::move(Candidate));
}

FImportTaskInfo FAssetImportWorkspace::SubmitDraft(const FImportDraftMutation& InRequest)
{
	auto Entry = MutableDraft(InRequest.Draft, InRequest.Generation);
	if (!Entry->Prepared)
	{
		throw FAssetImportError("unavailable", "Cannot submit a failed preparation");
	}
	auto Snapshot = std::make_shared<FPreparedImport>(*Entry->Prepared);
	Snapshot->Root = Entry->Edited;
	const auto Task = StartPrepared(Entry->Request, Snapshot, ImportPropertiesKey(Entry->History[Entry->Cursor]));
	Entry->Task = Task;
	Entry->Status = "publishing";
	++Entry->Generation;
	return Task->Info;
}

FImportDraftInfo FAssetImportWorkspace::DiscardDraft(const FImportDraftDiscard& InRequest)
{
	auto Entry = MutableDraft(InRequest.Draft, InRequest.Generation);
	if (!InRequest.bDiscard && ImportPropertiesKey(Entry->History[Entry->Cursor]) != Entry->SavedKey)
	{
		throw FAssetImportError("dirty", "Explicitly discard the modified import draft");
	}
	std::erase(Drafts, Entry);
	FImportDraftInfo Result;
	Result.Draft = Entry->Id;
	Result.Generation = Entry->Generation + 1;
	Result.Status = "discarded";
	return Result;
}
} // namespace Hyperion
