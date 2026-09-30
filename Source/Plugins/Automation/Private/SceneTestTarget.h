#pragma once
#include "Hyperion/SceneEditing/SceneDocument.h"

namespace Hyperion
{
class FSceneTestTarget final : public ISceneEditTarget
{
public:
	explicit FSceneTestTarget(FTaskSystem& InTasks) : Tasks(InTasks)
	{
	}

	FTaskSystem& Tasks;
	FScene Scene;
	bool bRejectAdd{};
	bool bRejectRebind{};
	std::size_t RebindCount{};
	mutable std::size_t NodesCount{};
	mutable std::size_t FindHandleCount{};

	bool IsLoaded() const override
	{
		return true;
	}

	bool IsReady() const override
	{
		return true;
	}

	std::uint64_t Identity() const override
	{
		return Scene.GetIdentity();
	}

	std::uint64_t Revision() const override
	{
		return Scene.GetRevision();
	}

	const FSceneNode* FindNode(FSceneHandle InHandle) const override
	{
		return Scene.FindNode(InHandle);
	}

	FSceneHandle FindHandle(std::string_view InId) const override
	{
		++FindHandleCount;
		return Scene.FindHandle(InId);
	}

	std::vector<FSceneHandle> Nodes() const override
	{
		++NodesCount;
		return Scene.GetNodes();
	}

	std::vector<FSceneHandle> Children(FSceneHandle InHandle) const override
	{
		return Scene.GetChildren(InHandle);
	}

	bool NodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const override
	{
		return Scene.GetNodeView(InHandle, OutView);
	}

	const FSceneSettings& Settings() const override
	{
		return Scene.GetSettings();
	}

	bool EditNodes(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision) override
	{
		return Scene.EditNodes(std::move(InEdits), InExpectedRevision);
	}

	FSceneHandle AddNode(FSceneNode InNode) override
	{
		return Scene.AddNode(std::move(InNode));
	}

	FSceneHandle DuplicateNode(FSceneHandle InHandle) override
	{
		const auto* Source = Scene.FindNode(InHandle);
		if (!Source)
		{
			return {};
		}
		auto Node = *Source;
		Node.Id.clear();
		return Scene.AddNode(std::move(Node));
	}

	bool RemoveNodeKeepChildren(FSceneHandle InHandle) override
	{
		return Scene.RemoveNodeKeepChildren(InHandle);
	}

	std::vector<FSceneHandle> AddNodes(std::vector<FSceneNode> InNodes,
	                                   std::vector<FSceneNodeEdit> InRestoredChildren = {}) override
	{
		if (bRejectAdd)
		{
			throw std::runtime_error("Injected resource admission failure");
		}
		return Scene.AddNodes(std::move(InNodes), std::move(InRestoredChildren));
	}

	bool RemoveSubtrees(std::span<const FSceneHandle> InRoots) override
	{
		return Scene.RemoveSubtrees(InRoots);
	}

	void SetSettings(FSceneSettings InSettings) override
	{
		Scene.SetSettings(std::move(InSettings));
	}

	FSceneNode Rebind(FSceneNode InNode) override
	{
		++RebindCount;
		if (bRejectRebind)
		{
			throw std::runtime_error("Injected resource rebind failure");
		}
		return InNode;
	}

	void RefreshAssets() override
	{
	}

	TAsyncResult<bool> Save(const std::filesystem::path&) override
	{
		return DispatchAsync<bool>(Tasks, {EDomain::Main},
		                           []
		                           {
			                           return true;
		                           });
	}
};

} // namespace Hyperion
