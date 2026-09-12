#pragma once
#include "Hyperion/Renderer/Model.h"
#include <set>

namespace Hyperion
{
class FRenderSession;

// Main-only attachment. Destroy before its session and logical scene.
class FSceneRenderBridge
{
public:
	FSceneRenderBridge(FScene& InScene, FRenderSession& InSession, FTaskSystem& InTasks);
	~FSceneRenderBridge();
	FSceneRenderBridge(const FSceneRenderBridge&) = delete;
	FSceneRenderBridge& operator=(const FSceneRenderBridge&) = delete;
	void Flush();
	void Close();
	FScenePublicationToken GetToken() const;
	FTaskHandle GetReceipt() const;
	std::uint64_t GetModelPreparationCount() const;
	std::string GetSceneError() const;
	bool IsReady(FSceneHandle InHandle) const;
	std::string GetError(FSceneHandle InHandle) const;
	std::vector<FRenderDrawResult> GetDrawResults(FSceneHandle InHandle) const;
	std::size_t PrimitiveCount(FSceneHandle InHandle) const;
	std::pair<std::uint64_t, std::uint64_t> GetStatusRevision() const;

private:
	struct FAttachment
	{
		std::shared_ptr<const FSceneModelData> Data;
		std::unique_ptr<FModel> Model;
		std::string Error;
		std::uint64_t Revision{};
		std::vector<std::pair<std::uint64_t, std::uint64_t>> MaterialVersions;
		std::vector<std::pair<std::shared_ptr<FMaterialInstance>, std::uint64_t>> EditableMaterials;
	};

	struct FReceipt
	{
		FSceneHandle Handle;
		FTaskHandle Task;
		std::uint64_t Revision{};
	};

	struct FPending
	{
		FSceneHandle Handle;
		std::unique_ptr<FModel> NewModel;
		FModel* Model{};
		FModel::FPreparedUpdate Update;
		std::vector<std::pair<std::uint64_t, std::uint64_t>> Versions;
		std::vector<std::pair<std::shared_ptr<FMaterialInstance>, std::uint64_t>> EditableMaterials;
	};

	std::vector<FPending> PrepareChanges(const std::vector<FSceneChange>& InChanges,
	                                     std::set<FSceneHandle>& OutAffected);
	void PublishChanges(std::vector<FPending>& InPending, const std::set<FSceneHandle>& InAffected,
	                    std::shared_ptr<const FSceneMetadata> InMetadata);
	void CommitChanges(std::vector<FPending>& InPending, FRenderScenePublication& InPublication,
	                   const std::set<FSceneHandle>& InAffected);
	void Observe(bool bInWait);
	std::shared_ptr<const FSceneMetadata> PrepareMetadata(const std::vector<FSceneChange>& InChanges) const;
	void ObserveScene(bool bInWait);
	std::uint64_t AttachmentEpoch{};
	std::shared_ptr<const FSceneMetadata> Metadata;
	std::vector<FTaskHandle> SceneReceipts;
	FTaskHandle LatestReceipt;
	std::string SceneError;
	FScene& Scene;
	FRenderSession& Session;
	FTaskSystem& Tasks;
	std::map<FSceneHandle, FAttachment> Attachments;
	std::vector<FReceipt> Receipts;
	std::set<FSceneHandle> EditableModels;
	std::uint64_t StatusRevision = 1;
	std::uint64_t ModelPreparationCount{};
	bool bClosed{};
	std::uint64_t NextPublication{};
};
} // namespace Hyperion
