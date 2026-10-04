#pragma once
#include "Hyperion/AssetEditing/AssetProperties.h"
#include "Hyperion/AssetEditing/AssetWorkflowErrors.h"

namespace Hyperion
{
struct FPreparedAssetField;

class FAssetWorkflowError : public FCodedError
{
public:
	FAssetWorkflowError(FErrorCode InCode, std::string InMessage) : FCodedError(std::move(InCode), std::move(InMessage))
	{
	}
};

// Main-owned operation. Workers hold only owned snapshots; the owner drains before releasing Tasks/Assets.
class FAssetEditWorkflow
{
public:
	FAssetEditWorkflow(const FAssetEditWorkflow&) = delete;
	FAssetEditWorkflow& operator=(const FAssetEditWorkflow&) = delete;
	FAssetEditWorkflow(FAssetEditWorkflow&&) = delete;
	FAssetEditWorkflow& operator=(FAssetEditWorkflow&&) = delete;
	static std::shared_ptr<FAssetEditWorkflow> Encoding(FTaskSystem& InTasks,
	                                                    std::shared_ptr<FAssetEditDocument> InDocument,
	                                                    std::uint64_t InGeneration,
	                                                    EMaterialTextureEncoding InEncoding);
	static std::shared_ptr<FAssetEditWorkflow> Field(FTaskSystem& InTasks, FAssetService& InAssets,
	                                                 std::shared_ptr<FAssetEditDocument> InDocument,
	                                                 std::uint64_t InGeneration, std::string InField,
	                                                 FArchiveNode InValue);
	static std::shared_ptr<FAssetEditWorkflow> Field(FTaskSystem& InTasks, FAssetService& InAssets,
	                                                 std::shared_ptr<FAssetEditDocument> InDocument,
	                                                 std::uint64_t InGeneration, const FRecordMemberIdentity& InField,
	                                                 FArchiveNode InValue);
	~FAssetEditWorkflow();
	bool Poll(const std::shared_ptr<FAssetEditDocument>& InCurrent);
	void Drain();
	bool IsPending() const;

private:
	FAssetEditWorkflow(FTaskSystem& InTasks, std::shared_ptr<FAssetEditDocument> InDocument,
	                   std::uint64_t InGeneration);
	void Release();
	FTaskSystem& Tasks;
	std::shared_ptr<FAssetEditDocument> Document;
	std::uint64_t Generation{};
	std::string AssetId;
	FRecordMemberIdentity FieldIdentity;
	std::unique_ptr<FPreparedAssetField> Prepared;
	std::optional<TAsyncResult<FArchiveNode>> EncodingResult;
	std::vector<TAsyncResult<FAssetGraph>> Graphs;
	bool bPending = true;
};
} // namespace Hyperion
