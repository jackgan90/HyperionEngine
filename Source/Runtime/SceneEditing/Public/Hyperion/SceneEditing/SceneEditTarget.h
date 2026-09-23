#pragma once
#include "Hyperion/Scene/Scene.h"
#include "Hyperion/Tasks/AsyncResult.h"
#include <filesystem>

namespace Hyperion
{
// Main-owned bridge to logical scene edits and resource effects. No renderer/native types cross this API.
class ISceneEditTarget
{
public:
	virtual ~ISceneEditTarget() = default;
	virtual bool IsLoaded() const = 0;
	virtual bool IsReady() const = 0;

	// Terminal resource failures must remain editable so callers can repair references.
	virtual bool IsPreparing() const
	{
		return !IsReady();
	}

	virtual std::uint64_t Identity() const = 0;
	virtual std::uint64_t Revision() const = 0;
	virtual const FSceneNode* FindNode(FSceneHandle InHandle) const = 0;
	virtual FSceneHandle FindHandle(std::string_view InId) const = 0;
	virtual std::vector<FSceneHandle> Nodes() const = 0;
	virtual std::vector<FSceneHandle> Children(FSceneHandle InHandle) const = 0;
	virtual bool NodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const = 0;
	virtual const FSceneSettings& Settings() const = 0;
	virtual bool EditNodes(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision) = 0;
	virtual FSceneHandle AddNode(FSceneNode InNode) = 0;
	virtual FSceneHandle DuplicateNode(FSceneHandle InHandle) = 0;
	virtual bool RemoveNodeKeepChildren(FSceneHandle InHandle) = 0;
	virtual std::vector<FSceneHandle> AddNodes(std::vector<FSceneNode> InNodes) = 0;
	virtual bool RemoveSubtrees(std::span<const FSceneHandle> InRoots) = 0;
	virtual void SetSettings(FSceneSettings InSettings) = 0;
	virtual FSceneNode Rebind(FSceneNode InNode) = 0;
	virtual void RefreshAssets() = 0;
	virtual TAsyncResult<bool> Save(const std::filesystem::path& InPath) = 0;
};
} // namespace Hyperion
