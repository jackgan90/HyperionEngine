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
	// Commits immediately when preparation needs no reference graph; otherwise returns the pending edit.
	static std::shared_ptr<FAssetEditWorkflow> SubmitField(FTaskSystem& InTasks, FAssetService& InAssets,
	                                                       std::shared_ptr<FAssetEditDocument> InDocument,
	                                                       std::uint64_t InGeneration,
	                                                       const FRecordMemberIdentity& InField, FArchiveNode InValue,
	                                                       std::uint64_t InInteraction = 0);
	~FAssetEditWorkflow();
	bool Poll(const std::shared_ptr<FAssetEditDocument>& InCurrent);
	void Drain();
	bool IsPending() const;
	std::uint64_t Interaction() const;
	bool CanContinueInteraction() const;
	const FArchiveNode* PreparedField(const FRecordMemberIdentity& InField) const;
	// Updates one live gesture without replacing its in-flight reference graphs or publishing the draft.
	void UpdateField(const FRecordMemberIdentity& InField, FArchiveNode InValue, std::uint64_t InInteraction);
	void FinishInteraction();
	void CancelInteraction(std::uint64_t InInteraction);

private:
	FAssetEditWorkflow(FTaskSystem& InTasks, std::shared_ptr<FAssetEditDocument> InDocument,
	                   std::uint64_t InGeneration);
	static std::shared_ptr<FAssetEditWorkflow> PrepareField(FTaskSystem& InTasks, FAssetService& InAssets,
	                                                        std::shared_ptr<FAssetEditDocument> InDocument,
	                                                        std::uint64_t InGeneration,
	                                                        const FRecordMemberIdentity& InField, FArchiveNode InValue,
	                                                        std::uint64_t InInteraction, bool bInDeferCommit);
	void Release();
	FTaskSystem& Tasks;
	std::shared_ptr<FAssetEditDocument> Document;
	std::uint64_t Generation{};
	std::string AssetId;
	FRecordMemberIdentity FieldIdentity;
	std::unique_ptr<FPreparedAssetField> Prepared;
	std::optional<TAsyncResult<FArchiveNode>> EncodingResult;
	std::vector<TAsyncResult<FAssetGraph>> Graphs;
	std::uint64_t InteractionId{};
	bool bInteractionFinished{};
	bool bCancelled{};
	bool bPending = true;
};
} // namespace Hyperion
