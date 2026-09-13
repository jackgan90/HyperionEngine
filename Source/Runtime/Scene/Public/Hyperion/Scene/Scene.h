#pragma once
#include "Hyperion/Scene/SceneNode.h"

namespace Hyperion
{
class FSceneStorage;

// Main-owned logical scene. Derived state is current after every successful transaction.
// Const pointers are Main-only borrows, invalidated by subsequent mutations.
class FScene
{
public:
	FScene();
	~FScene();
	FScene(const FScene&) = delete;
	FScene& operator=(const FScene&) = delete;
	void RequireMain() const;
	std::uint64_t GetIdentity() const;
	std::uint64_t GetRevision() const;
	FSceneHandle AddNode(FSceneNode InNode);
	// Batch installation requires an empty scene; returned handles follow input order.
	// Full validation precedes installation, including cycles and inherited poses.
	std::vector<FSceneHandle> LoadNodes(std::vector<FSceneNode> InNodes);
	bool RemoveSubtree(FSceneHandle InHandle);
	bool RemoveNodeKeepChildren(FSceneHandle InHandle);
	const FSceneNode* FindNode(FSceneHandle InHandle) const;
	const FSceneNode* FindNode(std::string_view InId) const;
	FSceneHandle FindHandle(std::string_view InId) const;
	bool GetNodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const;
	bool GetWorld(FSceneHandle InHandle, FMat4& OutWorld) const;
	bool IsEffectivelyEnabled(FSceneHandle InHandle) const;
	const FSceneCamera* FindCamera(FSceneHandle InHandle) const;
	const FSceneDirectionalLight* FindDirectionalLight(FSceneHandle InHandle) const;
	const FSceneEnvironmentLight* FindEnvironmentLight(FSceneHandle InHandle) const;
	const FSceneModelComponent* FindModelComponent(FSceneHandle InHandle) const;
	bool GetCameraPose(FSceneHandle InHandle, FSceneCameraPose& OutPose) const;
	std::vector<FSceneHandle> GetNodes() const;
	std::vector<FSceneHandle> GetNodes(ESceneNodeKind InKind) const;
	std::vector<FSceneHandle> GetRoots() const;
	std::vector<FSceneHandle> GetChildren(FSceneHandle InHandle) const;
	std::size_t CountNodes(ESceneNodeKind InKind) const;
	bool SetName(FSceneHandle InHandle, std::string InName);
	bool SetEnabled(FSceneHandle InHandle, bool bInEnabled);
	bool SetModelVisible(FSceneHandle InHandle, bool bInVisible);
	bool SetModelComponent(FSceneHandle InHandle, FSceneModelComponent InModel);
	bool SetCameraView(FSceneHandle InHandle, FMat4 InWorld, FSceneCamera InCamera);
	bool SetCamera(FSceneHandle InHandle, FSceneCamera InCamera);
	bool SetDirectionalLight(FSceneHandle InHandle, FSceneDirectionalLight InLight);
	bool SetEnvironmentLight(FSceneHandle InHandle, FSceneEnvironmentLight InLight);
	const FScenePointLight* FindPointLight(FSceneHandle InHandle) const;
	bool SetPointLight(FSceneHandle InHandle, FScenePointLight InLight);
	const FSceneSpotLight* FindSpotLight(FSceneHandle InHandle) const;
	bool SetSpotLight(FSceneHandle InHandle, FSceneSpotLight InLight);
	bool SetLocalTransform(FSceneHandle InHandle, FMat4 InLocal);
	bool SetWorldTransform(FSceneHandle InHandle, FMat4 InWorld);
	bool Reparent(FSceneHandle InHandle, std::optional<FSceneHandle> InParent, ESceneReparentMode InMode);
	bool SetSettings(FSceneSettings InSettings);
	const FSceneSettings& GetSettings() const;
	void Update();
	std::vector<FSceneChange> GetChanges() const;
	void Acknowledge(std::uint64_t InRevision);
	void Clear();
	void BeginSynchronization();
	void EndSynchronization();

	// Transitional model-only API; Update's World always means world, including under a parent.
	// Find projects effective visibility; Update preserves authored visibility when that bit is unchanged.
	// Use SetModelVisible to author visibility while a node or ancestor is disabled. No mutable cache is exposed.
	FSceneHandle Add(FSceneModel InModel);
	bool Update(FSceneHandle InHandle, FSceneModel InModel);
	bool Remove(FSceneHandle InHandle);
	const FSceneModel* Find(FSceneHandle InHandle) const;
	std::vector<FSceneHandle> GetHandles() const;

private:
	std::unique_ptr<FSceneStorage> Storage;
};

// Explicit initialization only. Empty FScene never creates cameras or lights implicitly.
std::array<FSceneHandle, 3> AddDefaultSceneContent(FScene& InScene, FVec3 InEye = {0, 3, 12}, FVec3 InTarget = {},
                                                   FSceneCamera InCamera = {});
} // namespace Hyperion
