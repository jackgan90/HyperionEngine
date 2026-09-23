#pragma once
#include "Hyperion/AssetEditing/AssetDocument.h"
#include "Hyperion/Automation/Catalog.h"
#include "Hyperion/Content/ContentRootService.h"

namespace Hyperion
{
struct FAssetOpenRequest
{
	std::string Path;
};

struct FAssetDocumentRequest
{
	std::string Document;
};

struct FAssetMutationRequest
{
	std::string Document;
	std::uint64_t Generation{};
};

struct FAssetRenameRequest
{
	std::string Document;
	std::uint64_t Generation{};
	std::string Name;
};

struct FAssetEncodingRequest
{
	std::string Document;
	std::uint64_t Generation{};
	EMaterialTextureEncoding Encoding{};
};

struct FAssetCloseRequest
{
	std::string Document;
	std::uint64_t Generation{};
	bool bDiscard{};
};

struct FAssetDocumentInfo
{
	std::string Document;
	std::string Path;
	std::string AssetId;
	std::string Type;
	std::string Name;
	std::uint64_t Generation{};
	bool bDirty{};
	bool bCanUndo{};
	bool bCanRedo{};
	bool bReadOnly{};
	bool bSaving{};
	bool bEditing{};
	std::string DiskRevision;
	std::vector<std::string> Fields;
};

struct FAssetCloseResult
{
	bool bClosed{};
};

template<> const FRecordDescriptor& RecordType<FAssetOpenRequest>();
template<> const FRecordDescriptor& RecordType<FAssetDocumentRequest>();
template<> const FRecordDescriptor& RecordType<FAssetMutationRequest>();
template<> const FRecordDescriptor& RecordType<FAssetRenameRequest>();
template<> const FRecordDescriptor& RecordType<FAssetEncodingRequest>();
template<> const FRecordDescriptor& RecordType<FAssetCloseRequest>();
template<> const FRecordDescriptor& RecordType<FAssetDocumentInfo>();
template<> const FRecordDescriptor& RecordType<FAssetCloseResult>();

class FAssetAutomation : public IContentRootParticipant
{
public:
	FAssetAutomation(FAssetService& InAssets, FTaskSystem& InTasks, FContentRootService* InRoots = nullptr);
	~FAssetAutomation();
	TPendingOperation<FAssetDocumentInfo> Open(const FAssetOpenRequest& InRequest);
	FAssetDocumentInfo Info(const FAssetDocumentRequest& InRequest);
	FAssetDocumentInfo Rename(const FAssetRenameRequest& InRequest);
	FAssetDocumentInfo Undo(const FAssetMutationRequest& InRequest);
	FAssetDocumentInfo Redo(const FAssetMutationRequest& InRequest);
	TPendingOperation<FAssetDocumentInfo> Save(const FAssetMutationRequest& InRequest);
	TPendingOperation<FAssetDocumentInfo> SetEncoding(const FAssetEncodingRequest& InRequest);
	FAssetCloseResult Close(const FAssetCloseRequest& InRequest);
	void Drain();
	FContentRootParticipantState ContentRootState() const override;
	void ReleaseContentRoot() override;
	void ContentRootChanged() override;

private:
	struct FEntry
	{
		std::string Id;
		std::filesystem::path Path;
		std::shared_ptr<FAssetEditDocument> Document;
		bool bEditing{};
	};

	std::shared_ptr<FEntry> Find(std::string_view InId);
	std::shared_ptr<FEntry> Edit(std::string_view InId, std::uint64_t InGeneration);
	FAssetDocumentInfo Describe(const FEntry& InEntry) const;
	FAssetService& Assets;
	FTaskSystem& Tasks;
	FContentRootService* Roots{};
	std::map<std::string, std::shared_ptr<FEntry>, std::less<>> Documents;
	std::vector<FTaskHandle> Work;
	std::string Identity;
	std::uint64_t NextDocument{};
};

void RegisterAssetOperations(FOperationCatalog& InCatalog, FAssetAutomation* InProvider);
void RegisterContentRootOperations(FOperationCatalog& InCatalog, FContentRootService* InRoots);
} // namespace Hyperion
