#pragma once
#include "Hyperion/Renderer/SceneInstance.h"
#include "Hyperion/SceneEditing/SceneEditTarget.h"

namespace Hyperion
{
class FSceneInstanceEditTarget final : public ISceneEditTarget
{
public:
	FSceneInstanceEditTarget(FSceneInstance& InScene, FAssetService& InAssets);
	bool IsLoaded() const override;
	bool IsReady() const override;
	std::uint64_t Identity() const override;
	std::uint64_t Revision() const override;
	const FSceneNode* FindNode(FSceneHandle InHandle) const override;
	FSceneHandle FindHandle(std::string_view InId) const override;
	std::vector<FSceneHandle> Nodes() const override;
	std::vector<FSceneHandle> Children(FSceneHandle InHandle) const override;
	bool NodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const override;
	const FSceneSettings& Settings() const override;
	bool EditNodes(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision) override;
	FSceneHandle AddNode(FSceneNode InNode) override;
	FSceneHandle DuplicateNode(FSceneHandle InHandle) override;
	bool RemoveNodeKeepChildren(FSceneHandle InHandle) override;
	std::vector<FSceneHandle> AddNodes(std::vector<FSceneNode> InNodes) override;
	bool RemoveSubtrees(std::span<const FSceneHandle> InRoots) override;
	void SetSettings(FSceneSettings InSettings) override;
	FSceneNode Rebind(FSceneNode InNode) override;
	void RefreshAssets() override;
	TAsyncResult<bool> Save(const std::filesystem::path& InPath) override;

private:
	FSceneInstance& Scene;
	FAssetService& Assets;
};
} // namespace Hyperion
