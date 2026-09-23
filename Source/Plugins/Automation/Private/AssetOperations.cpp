#include "AssetOperations.h"
#include "Hyperion/Environment/SkyAsset.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/Model.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace Hyperion
{
FAssetAutomation::FAssetAutomation(FAssetService& InAssets, FTaskSystem& InTasks, FContentRootService* InRoots)
    : Assets(InAssets), Tasks(InTasks), Roots(InRoots)
{
	Identity = CreateAutomationIdentity();
}

FAssetAutomation::~FAssetAutomation()
{
	Drain();
}

FContentRootParticipantState FAssetAutomation::ContentRootState() const
{
	FContentRootParticipantState Result;
	for (const auto& [Id, Entry] : Documents)
	{
		Result.bBusy |= !Entry->Document || Entry->bEditing || (Entry->Document && Entry->Document->IsSaving());
		Result.bDirty |= Entry->Document && Entry->Document->IsDirty();
	}
	return Result;
}

void FAssetAutomation::ReleaseContentRoot()
{
	Drain();
	Documents.clear();
}

void FAssetAutomation::ContentRootChanged()
{
}

void FAssetAutomation::Drain()
{
	for (const auto& Task : Work)
	{
		try
		{
			Tasks.Wait(Task);
		}
		catch (...)
		{ /* Jobs report operation failures. Still join every task. */
		}
	}
	Work.clear();
	for (const auto& [Id, Entry] : Documents)
	{
		while (Entry->Document && Entry->Document->IsSaving())
		{
			Tasks.PumpMain();
			Entry->Document->PollSave();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}

std::shared_ptr<FAssetAutomation::FEntry> FAssetAutomation::Find(std::string_view InId)
{
	if (Roots)
	{
		(void)Roots->Info();
	}
	const auto It = Documents.find(InId);
	if (It == Documents.end())
	{
		throw FAutomationError("not_found", "Unknown, closed or foreign-session document", "document");
	}
	if (!It->second->Document)
	{
		throw FAutomationError("busy", "Document is still opening", "document");
	}
	return It->second;
}

std::shared_ptr<FAssetAutomation::FEntry> FAssetAutomation::Edit(std::string_view InId, std::uint64_t InGeneration)
{
	auto Entry = Find(InId);
	if (Entry->Document->Generation() != InGeneration)
	{
		throw FAutomationError("stale_revision", "Document changed; query asset.info and retry with its generation",
		                       "generation");
	}
	if (Entry->bEditing)
	{
		throw FAutomationError("busy", "Texture edit is still running");
	}
	if (IsAssetPathReadOnly(Assets, Entry->Path))
	{
		throw FAutomationError("read_only", "Asset mount is read-only");
	}
	return Entry;
}

FAssetDocumentInfo FAssetAutomation::Describe(const FEntry& InEntry) const
{
	const auto& Document = *InEntry.Document;
	const auto& Loaded = Document.Loaded();
	FAssetDocumentInfo Result{InEntry.Id,
	                          PathToUtf8(InEntry.Path),
	                          Loaded.Header.Id,
	                          Loaded.Header.TypeId,
	                          ReadValue<std::string>(Document.Get("name")),
	                          Document.Generation(),
	                          Document.IsDirty(),
	                          Document.CanUndo(),
	                          Document.CanRedo(),
	                          IsAssetPathReadOnly(Assets, InEntry.Path),
	                          Document.IsSaving(),
	                          InEntry.bEditing,
	                          Loaded.Header.Revision};
	for (const auto& Member : Loaded.Type->Members)
	{
		Result.Fields.push_back(Member.Id);
	}
	return Result;
}

TPendingOperation<FAssetDocumentInfo> FAssetAutomation::Open(const FAssetOpenRequest& InRequest)
{
	if (Roots && Roots->Info().Directory.empty() && (InRequest.Path == "/Game" || InRequest.Path.starts_with("/Game/")))
	{
		throw FAutomationError("root_unset",
		                       "Select an asset directory with content.root.set before opening Game assets");
	}
	const auto Path = Assets.NormalizePath(PathFromUtf8(InRequest.Path));
	for (const auto& [Id, Existing] : Documents)
	{
		if (Existing->Path == Path)
		{
			if (!Existing->Document)
			{
				throw FAutomationError("busy", "Asset is already opening");
			}
			return {[this, Existing]
			        {
				        return std::optional(Describe(*Existing));
			        }};
		}
	}
	if (Documents.size() >= 64)
	{
		throw FAutomationError("busy", "Close a document before opening another (limit 64)");
	}
	auto Entry = std::make_shared<FEntry>();
	Entry->Id = Identity + "/document/" + std::to_string(++NextDocument);
	Entry->Path = Path;
	Documents.emplace(Entry->Id, Entry);
	try
	{
		std::erase_if(Work,
		              [](const auto& InTask)
		              {
			              return InTask.Ready();
		              });
		Work.reserve(Work.size() + 1);
		Assets.Invalidate(Path);
		auto Load = Assets.LoadAsync(Path);
		Work.push_back(Load.Task());
		return {[this, Entry, Load]() -> std::optional<FAssetDocumentInfo>
		        {
			        if (!Load.Ready())
			        {
				        return {};
			        }
			        try
			        {
				        auto Loaded = Load.GetReady();
				        const auto Type = Loaded->Type->CppType;
				        if (Type != typeid(FModelAsset) && Type != typeid(FMaterialAsset) &&
				            Type != typeid(FTextureAsset) && Type != typeid(FSkyAsset))
				        {
					        throw FAutomationError("unsupported_type",
					                               "This adapter opens model, material, texture and sky documents; "
					                               "scene editing is not yet registered");
				        }
				        Entry->Document = std::make_shared<FAssetEditDocument>(std::move(Loaded));
				        return Describe(*Entry);
			        }
			        catch (...)
			        {
				        Documents.erase(Entry->Id);
				        throw;
			        }
		        }};
	}
	catch (...)
	{
		Documents.erase(Entry->Id);
		throw;
	}
}

FAssetDocumentInfo FAssetAutomation::Info(const FAssetDocumentRequest& InRequest)
{
	return Describe(*Find(InRequest.Document));
}

FAssetDocumentInfo FAssetAutomation::Rename(const FAssetRenameRequest& InRequest)
{
	auto Entry = Edit(InRequest.Document, InRequest.Generation);
	Entry->Document->Set("name", WriteValue(InRequest.Name));
	return Describe(*Entry);
}

FAssetDocumentInfo FAssetAutomation::Undo(const FAssetMutationRequest& InRequest)
{
	auto Entry = Edit(InRequest.Document, InRequest.Generation);
	Entry->Document->Undo();
	return Describe(*Entry);
}

FAssetDocumentInfo FAssetAutomation::Redo(const FAssetMutationRequest& InRequest)
{
	auto Entry = Edit(InRequest.Document, InRequest.Generation);
	Entry->Document->Redo();
	return Describe(*Entry);
}

TPendingOperation<FAssetDocumentInfo> FAssetAutomation::Save(const FAssetMutationRequest& InRequest)
{
	auto Entry = Edit(InRequest.Document, InRequest.Generation);
	if (Entry->Document->IsSaving())
	{
		throw FAutomationError("busy", "A save is already running");
	}
	Entry->Document->Save(Assets);
	return {[this, Entry]() -> std::optional<FAssetDocumentInfo>
	        {
		        Entry->Document->PollSave();
		        if (Entry->Document->IsSaving())
		        {
			        return {};
		        }
		        if (!Entry->Document->Error.empty())
		        {
			        throw FAutomationError("save_failed", Entry->Document->Error);
		        }
		        return Describe(*Entry);
	        }};
}

TPendingOperation<FAssetDocumentInfo> FAssetAutomation::SetEncoding(const FAssetEncodingRequest& InRequest)
{
	auto Entry = Edit(InRequest.Document, InRequest.Generation);
	if (Entry->Document->Loaded().Type->CppType != typeid(FTextureAsset))
	{
		throw FAutomationError("unsupported_type", "Texture encoding requires a texture document");
	}
	if (!CanEditTextureEncoding(*Entry->Document->Loaded().As<FTextureAsset>()))
	{
		throw FAutomationError("unsupported_type", "Encoding is fixed for floating-point textures and cube maps");
	}
	std::erase_if(Work,
	              [](const auto& InTask)
	              {
		              return InTask.Ready();
	              });
	Work.reserve(Work.size() + 1);
	auto Rebuild = DispatchAsync<FArchiveNode>(Tasks, {EDomain::Worker},
	                                           [Draft = Entry->Document->Snapshot(), Encoding = InRequest.Encoding]
	                                           {
		                                           return RebuildTextureEncodingDraft(Draft, Encoding);
	                                           });
	Work.push_back(Rebuild.Task());
	Entry->bEditing = true;
	return {[this, Entry, Rebuild]() -> std::optional<FAssetDocumentInfo>
	        {
		        if (!Rebuild.Ready())
		        {
			        return {};
		        }
		        Entry->bEditing = false;
		        Entry->Document->Set("", *Rebuild.GetReady());
		        return Describe(*Entry);
	        }};
}

FAssetCloseResult FAssetAutomation::Close(const FAssetCloseRequest& InRequest)
{
	auto Entry = Find(InRequest.Document);
	if (Entry->Document->Generation() != InRequest.Generation)
	{
		throw FAutomationError("stale_revision", "Document changed", "generation");
	}
	if (Entry->bEditing || Entry->Document->IsSaving())
	{
		throw FAutomationError("busy", "Wait for pending edits and saves before closing");
	}
	if (Entry->Document->IsDirty() && !InRequest.bDiscard)
	{
		throw FAutomationError("dirty_document", "Save first or explicitly set discard=true");
	}
	Documents.erase(Entry->Id);
	return {true};
}
} // namespace Hyperion
