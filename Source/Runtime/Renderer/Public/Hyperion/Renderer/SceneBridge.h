#pragma once
#include "Hyperion/Renderer/Model.h"
#include "Hyperion/Renderer/SceneDiagnostics.h"
#include <set>

namespace Hyperion
{
class FRenderSession;

struct FSceneBridgeStatusRevision
{
	std::uint64_t BridgeStatus{};
	std::uint64_t ResourcePublication{};
	bool operator==(const FSceneBridgeStatusRevision&) const = default;
};

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
	std::vector<FRenderPrimitiveHandle> ResolveRenderPrimitives(FSceneHandle InHandle) const;
	std::shared_ptr<const FSceneComponentDiagnostics> GetComponentDiagnostics(FSceneHandle InHandle,
	                                                                          std::string_view InComponent) const;
	FSceneBridgeStatusRevision GetStatusRevision() const;
	FSceneBridgeStatusRevision GetModelStatusRevision() const;

private:
	struct FMaterialSnapshotRevision
	{
		std::uint64_t Identity{};
		std::uint64_t Revision{};
		bool operator==(const FMaterialSnapshotRevision&) const = default;
	};

	struct FSectionMaterialRevision
	{
		std::uint32_t SectionIndex{};
		FMaterialSnapshotRevision Snapshot;
		bool operator==(const FSectionMaterialRevision&) const = default;
	};

	struct FMaterialSelectionRevisions
	{
		FMaterialSnapshotRevision Surface;
		std::vector<FSectionMaterialRevision> Sections;
		bool operator==(const FMaterialSelectionRevisions&) const = default;
	};

	struct FEditableMaterialRevision
	{
		std::shared_ptr<FMaterialInstance> Instance;
		std::uint64_t Revision{};
	};

	struct FAttachment
	{
		std::shared_ptr<const FSceneModelData> Data;
		std::unique_ptr<FModel> Model;
		std::string Error;
		std::uint64_t Revision{};
		FMaterialSelectionRevisions MaterialRevisions;
		std::vector<FEditableMaterialRevision> EditableMaterials;
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
		FMaterialSelectionRevisions MaterialRevisions;
		std::vector<FEditableMaterialRevision> EditableMaterials;
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
	std::uint64_t ModelStatusRevision = 1;
	std::uint64_t ModelPreparationCount{};
	bool bClosed{};
	std::uint64_t NextPublication{};
};
} // namespace Hyperion
