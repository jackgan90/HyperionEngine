#pragma once
#include "Hyperion/Renderer/SceneBridge.h"
#include "Hyperion/Scene/SceneManifest.h"

namespace Hyperion
{
class FAssetService;

struct FSceneInstanceModel
{
	FSceneHandle Handle;
	std::string Asset;
	std::string Id;
};

struct FSceneInstanceAsset
{
	std::string Id;
	std::shared_ptr<const FSceneModelData> Data;
	std::string Error;
};

struct FSceneInstanceStatus
{
	std::size_t Nodes{};
	std::size_t Groups{};
	std::size_t Cameras{};
	std::size_t DirectionalLights{};
	std::size_t EnvironmentLights{};
	std::size_t Models{};
	std::size_t ReadyModels{};
	std::size_t FailedModels{};
	bool bLoaded{};
	bool bReady{};
	bool bClosed{};
	std::string Error;
	std::string PublicationError;
	bool bHasActiveCamera{};
};

// Main-owned scene lifecycle. Close before destroying its session, tasks or asset service.
class FSceneInstance
{
public:
	FSceneInstance(FRenderSession& InSession, FTaskSystem& InTasks, FAssetService& InAssets);
	~FSceneInstance();
	FSceneInstance(const FSceneInstance&) = delete;
	FSceneInstance& operator=(const FSceneInstance&) = delete;
	void Load(const std::filesystem::path& InPath);
	void Tick();
	FScenePublicationToken GetToken() const;
	FTaskHandle GetReceipt() const;
	void Close();
	FSceneHandle AddNode(FSceneNode InNode);
	bool RemoveSubtree(FSceneHandle InHandle);
	bool RemoveNodeKeepChildren(FSceneHandle InHandle);
	const FSceneNode* FindNode(FSceneHandle InHandle) const;
	FSceneHandle FindHandle(std::string_view InId) const;
	bool GetNodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const;
	std::vector<FSceneHandle> GetNodes() const;
	std::vector<FSceneHandle> GetNodes(ESceneNodeKind InKind) const;
	std::vector<FSceneHandle> GetRoots() const;
	std::vector<FSceneHandle> GetChildren(FSceneHandle InHandle) const;
	bool GetCameraPose(FSceneHandle InHandle, FSceneCameraPose& OutPose) const;
	bool SetName(FSceneHandle InHandle, std::string InName);
	bool SetEnabled(FSceneHandle InHandle, bool bInEnabled);
	bool SetModelVisible(FSceneHandle InHandle, bool bInVisible);
	bool SetModelComponent(FSceneHandle InHandle, FSceneModelComponent InModel);
	bool SetCameraView(FSceneHandle InHandle, FMat4 InWorld, FSceneCamera InCamera);
	bool SetCamera(FSceneHandle InHandle, FSceneCamera InCamera);
	bool SetDirectionalLight(FSceneHandle InHandle, FSceneDirectionalLight InLight);
	bool SetEnvironmentLight(FSceneHandle InHandle, FSceneEnvironmentLight InLight);
	bool SetLocalTransform(FSceneHandle InHandle, FMat4 InLocal);
	bool SetWorldTransform(FSceneHandle InHandle, FMat4 InWorld);
	bool Reparent(FSceneHandle InHandle, std::optional<FSceneHandle> InParent, ESceneReparentMode InMode);
	bool SetSettings(FSceneSettings InSettings);
	const FSceneSettings& GetSettings() const;
	FSceneHandle Add(FSceneModel InModel, std::string InAsset = {});
	bool Update(FSceneHandle InHandle, FSceneModel InModel);
	bool Remove(FSceneHandle InHandle);
	const FSceneModel* Find(FSceneHandle InHandle) const;
	std::vector<FSceneHandle> GetHandles() const;
	std::span<const FSceneInstanceModel> GetModels() const;
	std::vector<FSceneInstanceAsset> GetAssets() const;
	std::shared_ptr<const FSceneManifest> GetManifest() const;
	FSceneManifest Snapshot(const std::filesystem::path& InDestination) const;
	const FSceneInstanceStatus& GetStatus() const;
	std::string GetError(FSceneHandle InHandle) const;
	std::vector<FRenderDrawResult> GetDrawResults(FSceneHandle InHandle) const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

} // namespace Hyperion
