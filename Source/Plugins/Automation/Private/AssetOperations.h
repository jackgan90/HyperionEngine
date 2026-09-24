#pragma once
#include "Hyperion/AssetEditing/AssetDocument.h"
#include "Hyperion/AssetEditing/AssetWorkspace.h"
#include "Hyperion/AssetEditing/MaterialNumeric.h"
#include "Hyperion/AssetEditing/ModelProperties.h"
#include "Hyperion/Automation/Catalog.h"
#include "Hyperion/Content/ContentRootService.h"

namespace Hyperion
{
class IAssetPreviewWorkspace;
void RegisterAssetPreviews(FOperationCatalog& InCatalog, IAssetPreviewWorkspace* InWorkspace);

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
	std::string State = "ready";
	std::string Error;
	bool bActive{};
};

struct FAssetCloseResult
{
	bool bClosed{};
};

struct FAssetDocumentList
{
	std::vector<FAssetDocumentInfo> Documents;
	std::uint64_t Total{};
	std::optional<std::uint32_t> Next;
};

struct FAssetWorkspaceQuery
{
	std::uint32_t Offset{};
	std::uint32_t Limit = 50;
};

struct FAssetWorkspacePolicy
{
	bool bShared{};
	bool bRetainsFailed{};
	bool bRetainsLoading{};
	bool bActivation{};
};

template<> const FRecordDescriptor& RecordType<FAssetWorkspacePolicy>();

struct FTextureSampleRequest
{
	std::string Document;
	std::uint64_t Generation{};
	std::uint32_t Mip{};
	std::uint32_t Face{};
	std::uint32_t X{};
	std::uint32_t Y{};
};

struct FTextureSample
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::uint32_t Mips{};
	std::uint32_t Faces{};
	std::uint64_t MipBytes{};
	std::array<float, 4> Rgba{};
};

template<> const FRecordDescriptor& RecordType<FAssetOpenRequest>();
template<> const FRecordDescriptor& RecordType<FAssetDocumentRequest>();
template<> const FRecordDescriptor& RecordType<FAssetMutationRequest>();
template<> const FRecordDescriptor& RecordType<FAssetRenameRequest>();
template<> const FRecordDescriptor& RecordType<FAssetEncodingRequest>();
template<> const FRecordDescriptor& RecordType<FAssetCloseRequest>();
template<> const FRecordDescriptor& RecordType<FAssetDocumentInfo>();
template<> const FRecordDescriptor& RecordType<FAssetCloseResult>();
template<> const FRecordDescriptor& RecordType<FAssetDocumentList>();
template<> const FRecordDescriptor& RecordType<FAssetWorkspaceQuery>();

class FAssetAutomation : public IContentRootParticipant
{
public:
	FAssetAutomation(FAssetService& InAssets, FTaskSystem& InTasks, FContentRootService* InRoots = nullptr,
	                 IAssetWorkspace* InWorkspace = nullptr);
	~FAssetAutomation();
	TPendingOperation<FAssetDocumentInfo> Open(const FAssetOpenRequest& InRequest);
	FAssetDocumentInfo Info(const FAssetDocumentRequest& InRequest);
	FAssetDocumentInfo Rename(const FAssetRenameRequest& InRequest);
	FAssetDocumentInfo Undo(const FAssetMutationRequest& InRequest);
	FAssetDocumentInfo Redo(const FAssetMutationRequest& InRequest);
	TPendingOperation<FAssetDocumentInfo> Save(const FAssetMutationRequest& InRequest);
	TPendingOperation<FAssetDocumentInfo> SetEncoding(const FAssetEncodingRequest& InRequest);
	FAssetCloseResult Close(const FAssetCloseRequest& InRequest);
	FAssetDocumentList List(const FAssetWorkspaceQuery& InRequest) const;

	FAssetWorkspacePolicy WorkspacePolicy() const
	{
		return {Workspace != nullptr, Workspace != nullptr, Workspace != nullptr, Workspace != nullptr};
	}

	FAssetDocumentInfo Activate(const FAssetDocumentRequest& InRequest);
	FTextureSample TextureSample(const FTextureSampleRequest& InRequest);
	FArchiveNode ReadField(const std::string& InDocument, std::string_view InType, std::string_view InField);
	TPendingOperation<FAssetDocumentInfo> SetField(const std::string& InDocument, std::uint64_t InGeneration,
	                                               std::string_view InType, std::string InField, FArchiveNode InValue);
	void Drain();
	FMaterialNumericInfo MaterialNumeric(const FAssetMutationRequest& InRequest, const std::string& InName);
	FAssetDocumentInfo SetMaterialNumeric(const FAssetMutationRequest& InRequest,
	                                      const std::vector<FMaterialNumericEdit>& InEdits);
	std::vector<FModelPrimitiveInfo> ModelPrimitives(const FAssetMutationRequest& InRequest);
	FAssetDocumentInfo SetPrimitives(const FAssetMutationRequest& InRequest, std::uint32_t InOffset,
	                                 const std::vector<FModelPrimitiveInfo>& InValues);
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
	FAssetDocumentInfo DescribeWorkspace(const FAssetWorkspaceEntry& InEntry) const;
	FAssetService& Assets;
	FTaskSystem& Tasks;
	FContentRootService* Roots{};
	IAssetWorkspace* Workspace{};
	TPendingOperation<FAssetDocumentInfo> OpenWorkspace(const FAssetOpenRequest& InRequest);
	std::map<std::string, std::shared_ptr<FEntry>, std::less<>> Documents;
	std::vector<FTaskHandle> Work;
	std::string Identity;
	std::uint64_t NextDocument{};
};

void RegisterAssetOperations(FOperationCatalog& InCatalog, FAssetAutomation* InProvider);
void RegisterMaterialNumeric(FOperationCatalog& InCatalog, FAssetAutomation* InProvider);
void RegisterAssetProperties(FOperationCatalog& InCatalog, FAssetAutomation* InProvider);
void RegisterModelProperties(FOperationCatalog& InCatalog, FAssetAutomation* InProvider);
void RegisterTextureSamples(FOperationCatalog& InCatalog, FAssetAutomation* InProvider);
} // namespace Hyperion
