#include "AssetDocument.h"

namespace Hyperion
{
namespace
{
FArchiveNode& DocumentField(FArchiveNode& InDraft, std::string_view InField)
{
	return InField.empty()
	           ? InDraft
	           : std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InDraft.Value).at("fields").Value)
	                 .at(std::string(InField));
}
} // namespace

FArchiveNode RebuildTextureEncodingDraft(const FArchiveNode& InDraft, EMaterialTextureEncoding InEncoding)
{
	const auto& Fields =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(InDraft.Value).at("fields").Value);
	const auto& Base = std::get<FArchiveNode::FArray>(Fields.at("mips").Value).front();
	auto Result = WriteValue(
	    BuildTextureAsset(ReadValue<std::string>(Fields.at("name")), InEncoding, ReadValue<FMaterialTextureMip>(Base)));
	ShareAssetBulk(Result);
	// Encoding affects downsampling, never the stored top-level pixels.
	std::get<FArchiveNode::FArray>(DocumentField(Result, "mips").Value).front() = Base;
	return Result;
}

void ShareAssetBulk(FArchiveNode& InNode)
{
	if (auto* Bulk = std::get_if<FBulkData>(&InNode.Value); Bulk && !Bulk->Storage)
	{
		Bulk->Storage = std::make_shared<const std::vector<std::byte>>(std::move(Bulk->Bytes));
		Bulk->Size = Bulk->Storage->size();
		Bulk->Offset = 0;
	}
	else if (auto* Array = std::get_if<FArchiveNode::FArray>(&InNode.Value))
	{
		for (auto& Child : *Array)
		{
			ShareAssetBulk(Child);
		}
	}
	else if (auto* Object = std::get_if<FArchiveNode::FObject>(&InNode.Value))
	{
		for (auto& [Key, Child] : *Object)
		{
			ShareAssetBulk(Child);
		}
	}
}

FAssetEditorDocument::FAssetEditorDocument(std::shared_ptr<const FLoadedAsset> InAsset) : Asset(std::move(InAsset))
{
	Draft = WriteRecord(*Asset->Type, Asset->Object.get());
	ShareAssetBulk(Draft);
}

const FArchiveNode& FAssetEditorDocument::Get(std::string_view InField) const
{
	return std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Draft.Value).at("fields").Value)
	    .at(std::string(InField));
}

void FAssetEditorDocument::Set(std::string InField, FArchiveNode InValue, std::uint64_t InInteraction,
                               bool bInAffectsPreview)
{
	bInAffectsPreview &= InField != "name";
	ShareAssetBulk(InValue);
	auto& Current = DocumentField(Draft, InField);
	if (InInteraction && ActiveInteraction == InInteraction && Cursor && Cursor == History.size() &&
	    History.back().Field == InField && State != SavedState && (!PendingSave || State != SubmittedState))
	{
		History.back().After = InValue;
		History.back().bAffectsPreview |= bInAffectsPreview;
	}
	else
	{
		History.resize(Cursor);
		History.push_back({std::move(InField), Current, InValue, State, ++NextState, InInteraction, bInAffectsPreview});
		++Cursor;
	}
	Current = std::move(InValue);
	State = History.back().AfterState;
	ActiveInteraction = InInteraction;
	if (bInAffectsPreview)
	{
		++PreviewRevision;
	}
	++Revision;
	Error.clear();
}

void FAssetEditorDocument::FinishInteraction()
{
	ActiveInteraction = 0;
}

void FAssetEditorDocument::CancelInteraction(std::uint64_t InInteraction)
{
	if (InInteraction && InInteraction != ActiveInteraction)
	{
		return;
	}
	if (ActiveInteraction && Cursor == History.size() && Cursor)
	{
		Undo();
		History.resize(Cursor);
	}
	FinishInteraction();
}

bool FAssetEditorDocument::Undo()
{
	FinishInteraction();
	if (!CanUndo())
	{
		return false;
	}
	const auto& Edit = History[--Cursor];
	DocumentField(Draft, Edit.Field) = Edit.Before;
	State = Edit.BeforeState;
	if (Edit.bAffectsPreview)
	{
		++PreviewRevision;
	}
	++Revision;
	return true;
}

bool FAssetEditorDocument::Redo()
{
	FinishInteraction();
	if (!CanRedo())
	{
		return false;
	}
	const auto& Edit = History[Cursor++];
	DocumentField(Draft, Edit.Field) = Edit.After;
	State = Edit.AfterState;
	if (Edit.bAffectsPreview)
	{
		++PreviewRevision;
	}
	++Revision;
	return true;
}

bool FAssetEditorDocument::CanUndo() const
{
	return Cursor != 0;
}

bool FAssetEditorDocument::CanRedo() const
{
	return Cursor < History.size();
}

bool FAssetEditorDocument::IsDirty() const
{
	return State != SavedState;
}

std::uint64_t FAssetEditorDocument::Generation() const
{
	return Revision;
}

std::uint64_t FAssetEditorDocument::PreviewGeneration() const
{
	return PreviewRevision;
}

const FArchiveNode& FAssetEditorDocument::Snapshot() const
{
	return Draft;
}

const FLoadedAsset& FAssetEditorDocument::Loaded() const
{
	return *Asset;
}

bool FAssetEditorDocument::IsSaving() const
{
	return PendingSave.has_value();
}

void FAssetEditorDocument::Save(FAssetService& InAssets)
{
	if (PendingSave)
	{
		return;
	}
	FinishInteraction();
	const auto& Header = Asset->Header;
	PendingSave =
	    InAssets.SaveDocumentAsync(Asset->Path, *Asset->Type, Draft, {Header.Id, {}, Header.TypeId, Header.Revision});
	SubmittedState = State;
	Error.clear();
}

std::optional<FAssetSaveResult> FAssetEditorDocument::PollSave()
{
	if (!PendingSave || !PendingSave->Ready())
	{
		return {};
	}
	try
	{
		const auto Result = *PendingSave->GetReady();
		auto Updated = std::make_shared<FLoadedAsset>(*Asset);
		Updated->Header = Result.Header;
		Asset = std::move(Updated);
		SavedState = SubmittedState;
		PendingSave.reset();
		return Result;
	}
	catch (const std::exception& Failure)
	{
		Error = Failure.what();
		PendingSave.reset();
		return {};
	}
}
} // namespace Hyperion
