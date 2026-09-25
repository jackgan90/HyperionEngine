#pragma once
#include "Hyperion/AssetImport/AssetImportService.h"
#include "Hyperion/AssetImport/ImportDraft.h"
#include "Hyperion/Content/ContentRootService.h"
#include <thread>

namespace Hyperion
{
struct FImportRequest
{
	std::uint64_t Generation{};
	std::string Source;
	std::string Output;
	std::string Library;
	std::string Name;
	std::string Type;
	std::string SourceRoot;
	std::string SourceId;
	bool bScene{};
	bool bForce{};
	std::string RootId;
	std::optional<EMaterialTextureEncoding> TextureEncoding;
	std::optional<FEnvironmentBakeSettings> Sky;
};

struct FImportResult
{
	FAssetRef Asset;
	std::uint64_t WrittenAssets{};
	bool bUpToDate{};
	std::string Task;
	std::string Warning;
};

struct FImportValidation
{
	std::string Source;
	std::string Output;
	std::string Library;
	std::string Type;
};

struct FImportCapability
{
	std::string Type;
	std::vector<std::string> Extensions;
	std::string Description;
};

struct FImportCapabilities
{
	std::vector<FImportCapability> Formats;
	bool bCancellable{};
};

struct FImportTaskQuery
{
	std::string Task;
};

struct FImportTaskListRequest
{
	std::uint32_t Offset{};
	std::uint32_t Limit = 16;
};

struct FImportTaskInfo
{
	std::string Task;
	std::uint64_t Generation{};
	std::string Source;
	std::string Output;
	std::string Status = "running";
	std::optional<FImportResult> Result;
	std::string Error;
};

struct FImportTaskList
{
	std::vector<FImportTaskInfo> Tasks;
	std::uint32_t Total{};
};

class FAssetImportError : public std::runtime_error
{
public:
	FAssetImportError(std::string InCode, std::string InMessage)
	    : std::runtime_error(std::move(InMessage)), Code(std::move(InCode))
	{
	}

	std::string Code;
};

// Main-only records; shared ownership keeps automation polls valid when history is pruned.
struct FImportTask
{
	FImportTaskInfo Info;
	TAsyncResult<FAssetImportResult> Pending;
};

struct FImportDraft
{
	std::string Id;
	std::uint64_t Generation = 1;
	FImportRequest Request;
	std::string Status = "preparing";
	std::string Error;
	TAsyncResult<FPreparedImport> Pending;
	std::shared_ptr<const FPreparedImport> Prepared;
	FConvertedAsset Edited;
	std::vector<FImportPropertyEdits> History{{}};
	std::size_t Cursor{};
	std::string SavedKey;
	std::shared_ptr<const FImportTask> Task;
	mutable std::optional<FImportDraftInfo> Inspection;
	mutable FImportDraftQuery InspectionQuery;
};

class FAssetImportWorkspace final : public IContentRootParticipant
{
public:
	FAssetImportWorkspace(FIOService& InIO, FAssetService& InAssets, FContentRootService& InRoots);
	~FAssetImportWorkspace();
	static FImportCapabilities Capabilities();
	FImportValidation Validate(const FImportRequest& InRequest) const;
	std::shared_ptr<const FImportTask> Start(const FImportRequest& InRequest);
	FImportTaskInfo Get(const FImportTaskQuery& InRequest) const;
	FImportTaskList List(const FImportTaskListRequest& InRequest = {}) const;
	FImportDraftInfo PrepareDraft(const FImportRequest& InRequest);
	FImportDraftList DraftList() const;
	FImportDraftInfo Draft(const FImportDraftQuery& InRequest) const;
	FImportDraftInfo EditDraft(const FImportDraftEdit& InRequest);
	FImportDraftInfo DraftHistory(const FImportDraftHistory& InRequest);
	FImportTaskInfo SubmitDraft(const FImportDraftMutation& InRequest);
	FImportDraftInfo DiscardDraft(const FImportDraftDiscard& InRequest);
	void Update();
	void Drain();
	std::uint64_t Revision() const;
	FContentRootParticipantState ContentRootState() const override;
	void ReleaseContentRoot() override;
	void ContentRootChanged() override;

private:
	void RequireMain() const;
	void Complete(FImportTask& InTask);
	std::shared_ptr<FImportDraft> FindDraft(const std::string& InId) const;
	std::shared_ptr<FImportDraft> MutableDraft(const std::string& InId, std::uint64_t InGeneration) const;
	void UpdateDrafts();
	static FImportDraftInfo InspectDraft(const FImportDraft& InDraft, const FImportDraftQuery& InQuery);
	FImportDraftInfo CommitDraft(const std::shared_ptr<FImportDraft>& InDraft, FImportDraft InCandidate);
	std::shared_ptr<const FImportTask> StartPrepared(const FImportRequest& InRequest,
	                                                 std::shared_ptr<const FPreparedImport> InPrepared,
	                                                 std::string InOverrides);
	FAssetImportOptions Options(const FImportRequest& InRequest) const;
	FIOService& IO;
	FAssetService& Assets;
	FContentRootService& Roots;
	FAssetImportService Imports;
	std::thread::id Owner = std::this_thread::get_id();
	std::vector<std::shared_ptr<FImportTask>> Tasks;
	std::vector<std::shared_ptr<FImportDraft>> Drafts;
	std::uint64_t ContentRevision{};
	bool bClosing{};
};

template<> const FRecordDescriptor& RecordType<FImportRequest>();
template<> const FRecordDescriptor& RecordType<FImportResult>();
template<> const FRecordDescriptor& RecordType<FImportValidation>();
template<> const FRecordDescriptor& RecordType<FImportCapability>();
template<> const FRecordDescriptor& RecordType<FImportCapabilities>();
template<> const FRecordDescriptor& RecordType<FImportTaskQuery>();
template<> const FRecordDescriptor& RecordType<FImportTaskListRequest>();
template<> const FRecordDescriptor& RecordType<FImportTaskInfo>();
template<> const FRecordDescriptor& RecordType<FImportTaskList>();
} // namespace Hyperion
