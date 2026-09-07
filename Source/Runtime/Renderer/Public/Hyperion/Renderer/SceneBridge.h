#pragma once
#include "Hyperion/Renderer/Model.h"

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
	bool IsReady(FSceneHandle InHandle) const;
	std::string GetError(FSceneHandle InHandle) const;
	std::vector<FRenderDrawResult> GetDrawResults(FSceneHandle InHandle) const;
	std::size_t PrimitiveCount(FSceneHandle InHandle) const;

private:
	struct FAttachment
	{
		std::shared_ptr<const FSceneModelData> Data;
		std::unique_ptr<FModel> Model;
		std::string Error;
		std::uint64_t Revision{};
		std::vector<std::pair<std::uint64_t, std::uint64_t>> MaterialVersions;
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
	};

	std::vector<FPending> PrepareChanges(const std::vector<FSceneChange>& InChanges);
	void PublishChanges(std::vector<FPending>& InPending);
	void CommitChanges(std::vector<FPending>& InPending, FRenderScenePublication& InPublication);
	void Observe(bool bInWait);
	FScene& Scene;
	FRenderSession& Session;
	FTaskSystem& Tasks;
	std::map<FSceneHandle, FAttachment> Attachments;
	std::vector<FReceipt> Receipts;
	bool bClosed{};
	std::uint64_t NextPublication{};
};
} // namespace Hyperion
