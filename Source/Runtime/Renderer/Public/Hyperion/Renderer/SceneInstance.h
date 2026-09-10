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
};

struct FSceneInstanceAsset
{
	std::string Id;
	std::shared_ptr<const FSceneModelData> Data;
	std::string Error;
};

struct FSceneInstanceStatus
{
	std::size_t Models{};
	std::size_t ReadyModels{};
	std::size_t FailedModels{};
	bool bLoaded{};
	bool bReady{};
	bool bClosed{};
	std::string Error;
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
	void Close();
	FSceneHandle Add(FSceneModel InModel, std::string InAsset = {});
	bool Update(FSceneHandle InHandle, FSceneModel InModel);
	bool Remove(FSceneHandle InHandle);
	const FSceneModel* Find(FSceneHandle InHandle) const;
	std::vector<FSceneHandle> GetHandles() const;
	std::span<const FSceneInstanceModel> GetModels() const;
	std::vector<FSceneInstanceAsset> GetAssets() const;
	std::shared_ptr<const FSceneManifest> GetManifest() const;
	const FSceneInstanceStatus& GetStatus() const;
	std::string GetError(FSceneHandle InHandle) const;
	std::vector<FRenderDrawResult> GetDrawResults(FSceneHandle InHandle) const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};

void RegisterSceneManifestLoader(FAssetService& InAssets);
} // namespace Hyperion
