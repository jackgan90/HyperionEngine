#include "AssetOperations.h"
#include "Hyperion/Environment/SkyAsset.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
namespace
{
FOperationInfo AssetInfo(std::string InId, std::string InSummary, std::string InDescription, std::string InEffects,
                         std::string InCompletion, FArchiveNode InExample, bool bInReadOnly,
                         const FAssetAutomation* InProvider)
{
	return {std::move(InId),
	        std::move(InSummary),
	        std::move(InDescription),
	        "automation-assets",
	        std::move(InEffects),
	        std::move(InCompletion),
	        std::move(InExample),
	        1,
	        bInReadOnly,
	        {"asset", "document", "editor"},
	        InProvider ? "" : "Asset service is unavailable; enable assets with a valid Engine content directory"};
}

template<class T> FArchiveNode Example(const T& InValue)
{
	return WriteRecordWire(RecordType<T>(), &InValue);
}

void RegisterDocumentAccess(FOperationCatalog& InCatalog, FAssetAutomation* InProvider)
{
	const FAssetMutationRequest Mutation{"document-from-open", 1};
	InCatalog.Register(MakeAsyncOperation<FAssetOpenRequest, FAssetDocumentInfo>(
	    AssetInfo("asset.open", "Open an asset document",
	              "Open a native model, material, texture or sky. Reopening the same normalized path returns its "
	              "existing draft. Standalone limits ownership to 64 documents; attached Editor uses its actual shared "
	              "workspace.",
	              "Loads CPU data and creates a session document; does not write files.",
	              "CPU draft ready; no GPU preview is implied.",
	              Example(FAssetOpenRequest{"/Game/Materials/Example.hasset"}), false, InProvider),
	    [InProvider](const FAssetOpenRequest& InRequest)
	    {
		    return InProvider->Open(InRequest);
	    }));
	InCatalog.Register(MakeOperation<FAssetDocumentRequest, FAssetDocumentInfo>(
	    AssetInfo("asset.info", "Inspect document state",
	              "Returns identity, type, fields, generation and dirty/history/save state. Query types.describe with "
	              "the returned type for the data schema. Bulk data is excluded.",
	              "Reads the current draft state.", "Main snapshot returned.",
	              Example(FAssetDocumentRequest{Mutation.Document}), true, InProvider),
	    [InProvider](const FAssetDocumentRequest& InRequest)
	    {
		    return InProvider->Info(InRequest);
	    }));
	InCatalog.Register(MakeOperation<FAssetRenameRequest, FAssetDocumentInfo>(
	    AssetInfo("asset.rename", "Change asset display name",
	              "Uses the same document field transaction as Editor. Requires the current generation. Save "
	              "explicitly to persist.",
	              "Changes draft and history; no file rename or disk write.", "Draft changed on Main.",
	              Example(FAssetRenameRequest{Mutation.Document, 1, "Example"}), false, InProvider),
	    [InProvider](const FAssetRenameRequest& InRequest)
	    {
		    return InProvider->Rename(InRequest);
	    }));
}

void RegisterDocumentEditing(FOperationCatalog& InCatalog, FAssetAutomation* InProvider)
{
	const FAssetMutationRequest Mutation{"document-from-open", 1};
	InCatalog.Register(MakeOperation<FAssetMutationRequest, FAssetDocumentInfo>(
	    AssetInfo(
	        "asset.undo", "Undo a document edit",
	        "Undo one shared document transaction. Empty history is a successful no-op. Requires current generation.",
	        "Changes draft and history; no disk write.", "Draft updated on Main.", Example(Mutation), false,
	        InProvider),
	    [InProvider](const FAssetMutationRequest& InRequest)
	    {
		    return InProvider->Undo(InRequest);
	    }));
	InCatalog.Register(MakeOperation<FAssetMutationRequest, FAssetDocumentInfo>(
	    AssetInfo("asset.redo", "Redo a document edit",
	              "Redo one shared document transaction. Empty redo history is a successful no-op. Requires current "
	              "generation.",
	              "Changes draft and history; no disk write.", "Draft updated on Main.", Example(Mutation), false,
	              InProvider),
	    [InProvider](const FAssetMutationRequest& InRequest)
	    {
		    return InProvider->Redo(InRequest);
	    }));
	InCatalog.Register(MakeAsyncOperation<FAssetMutationRequest, FAssetDocumentInfo>(
	    AssetInfo(
	        "asset.save", "Save a captured document revision",
	        "Captures the draft at admission and checks disk identity/digest before publishing. Later edits remain "
	        "dirty. Concurrent saves are busy; conflicts return save_failed. Cannot cancel after admission.",
	        "Writes the native asset file through the shared asset service.",
	        "Captured draft persisted; returned state may contain later unsaved edits.", Example(Mutation), false,
	        InProvider),
	    [InProvider](const FAssetMutationRequest& InRequest)
	    {
		    return InProvider->Save(InRequest);
	    }));
	InCatalog.Register(MakeAsyncOperation<FAssetEncodingRequest, FAssetDocumentInfo>(
	    AssetInfo("texture.set_encoding", "Change encoding and rebuild texture mips",
	              "Uses Editor's CPU mip rebuild for 2D RGBA8 textures. Base pixels remain unchanged. Blocks other "
	              "edits until completion; save explicitly. Cannot cancel after admission.",
	              "Changes the draft mip chain and adds one undo transaction; no disk write.",
	              "New mip chain applied on Main.",
	              Example(FAssetEncodingRequest{Mutation.Document, 1, EMaterialTextureEncoding::Srgb}), false,
	              InProvider),
	    [InProvider](const FAssetEncodingRequest& InRequest)
	    {
		    return InProvider->SetEncoding(InRequest);
	    }));
	InCatalog.Register(MakeOperation<FAssetCloseRequest, FAssetCloseResult>(
	    AssetInfo("asset.close", "Close an idle asset document",
	              "Requires current generation and no pending save/edit. Dirty drafts require explicit discard=true. "
	              "Loading/failed workspace entries use generation 0 and can be closed then reopened to retry. "
	              "Closed IDs cannot be reused.",
	              "Releases the session draft; discard=true loses unsaved edits.", "Document removed.",
	              Example(FAssetCloseRequest{Mutation.Document, 1, false}), false, InProvider),
	    [InProvider](const FAssetCloseRequest& InRequest)
	    {
		    return InProvider->Close(InRequest);
	    }));
}
} // namespace

void RegisterAssetOperations(FOperationCatalog& InCatalog, FAssetAutomation* InProvider)
{
	InCatalog.Register(MakeOperation<FAssetWorkspaceQuery, FAssetDocumentList>(
	    AssetInfo("asset.documents.list", "List ready, loading and failed workspace entries",
	              "Lists workspace entries including GUI-opened, loading and failed tabs when attached. Non-ready "
	              "entries expose state/error and generation 0; close then reopen to retry. Closed IDs are invalid.",
	              "Reads shared workspace.", "Main snapshot.", Example(FAssetWorkspaceQuery{}), true, InProvider),
	    [InProvider](const auto& InRequest)
	    {
		    return InProvider->List(InRequest);
	    }));
	InCatalog.Register(MakeOperation<FAssetDocumentRequest, FAssetDocumentInfo>(
	    AssetInfo(
	        "asset.activate", "Activate an asset document",
	        "Activates an existing Editor asset tab. Standalone mode validates the document without a visual tab.",
	        "Changes active tab only.", "Main selection updated.", Example(FAssetDocumentRequest{"document-from-open"}),
	        false, InProvider),
	    [InProvider](const auto& InRequest)
	    {
		    return InProvider->Activate(InRequest);
	    }));
	for (const auto* Type : {&RecordType<FModelAsset>(), &RecordType<FMaterialAsset>(), &RecordType<FTextureAsset>(),
	                         &RecordType<FSkyAsset>()})
	{
		InCatalog.RegisterType(*Type);
	}
	RegisterDocumentAccess(InCatalog, InProvider);
	RegisterDocumentEditing(InCatalog, InProvider);
	RegisterAssetProperties(InCatalog, InProvider);
}
} // namespace Hyperion
