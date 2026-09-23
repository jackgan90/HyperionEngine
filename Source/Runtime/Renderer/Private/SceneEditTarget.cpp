#include "Hyperion/Renderer/SceneEditTarget.h"
#include "Hyperion/Assets/AssetService.h"

namespace Hyperion
{
FSceneInstanceEditTarget::FSceneInstanceEditTarget(FSceneInstance& InScene, FAssetService& InAssets)
    : Scene(InScene), Assets(InAssets)
{
}

bool FSceneInstanceEditTarget::IsLoaded() const
{
	const auto& Status = Scene.GetStatus();
	return Status.bLoaded && !Status.bClosed && Status.Error.empty();
}

bool FSceneInstanceEditTarget::IsReady() const
{
	return IsLoaded() && Scene.GetStatus().bReady;
}

std::uint64_t FSceneInstanceEditTarget::Identity() const
{
	return Scene.GetIdentity();
}

std::uint64_t FSceneInstanceEditTarget::Revision() const
{
	return Scene.GetRevision();
}

const FSceneNode* FSceneInstanceEditTarget::FindNode(FSceneHandle InHandle) const
{
	return Scene.FindNode(InHandle);
}

FSceneHandle FSceneInstanceEditTarget::FindHandle(std::string_view InId) const
{
	return Scene.FindHandle(InId);
}

std::vector<FSceneHandle> FSceneInstanceEditTarget::Nodes() const
{
	return Scene.GetNodes();
}

std::vector<FSceneHandle> FSceneInstanceEditTarget::Children(FSceneHandle InHandle) const
{
	return Scene.GetChildren(InHandle);
}

bool FSceneInstanceEditTarget::NodeView(FSceneHandle InHandle, FSceneNodeView& OutView) const
{
	return Scene.GetNodeView(InHandle, OutView);
}

const FSceneSettings& FSceneInstanceEditTarget::Settings() const
{
	return Scene.GetSettings();
}

bool FSceneInstanceEditTarget::EditNodes(std::vector<FSceneNodeEdit> InEdits, std::uint64_t InExpectedRevision)
{
	return Scene.EditNodes(std::move(InEdits), InExpectedRevision);
}

FSceneHandle FSceneInstanceEditTarget::AddNode(FSceneNode InNode)
{
	return Scene.AddNode(std::move(InNode));
}

std::vector<FSceneHandle> FSceneInstanceEditTarget::AddNodes(std::vector<FSceneNode> InNodes)
{
	return Scene.AddNodes(std::move(InNodes));
}

FSceneHandle FSceneInstanceEditTarget::DuplicateNode(FSceneHandle InHandle)
{
	return Scene.DuplicateNode(InHandle);
}

bool FSceneInstanceEditTarget::RemoveNodeKeepChildren(FSceneHandle InHandle)
{
	return Scene.RemoveNodeKeepChildren(InHandle);
}

bool FSceneInstanceEditTarget::RemoveSubtrees(std::span<const FSceneHandle> InRoots)
{
	return Scene.RemoveSubtrees(InRoots);
}

void FSceneInstanceEditTarget::SetSettings(FSceneSettings InSettings)
{
	Scene.SetSettings(std::move(InSettings));
}

FSceneNode FSceneInstanceEditTarget::Rebind(FSceneNode InNode)
{
	return Scene.RebindAssetResources(std::move(InNode));
}

void FSceneInstanceEditTarget::RefreshAssets()
{
	Scene.RefreshAssets();
}

TAsyncResult<bool> FSceneInstanceEditTarget::Save(const std::filesystem::path& InPath)
{
	auto Snapshot = std::make_shared<const FSceneManifest>(Scene.Snapshot(InPath));
	WriteRecord(RecordType<FSceneManifest>(), Snapshot.get());
	return Assets.SaveAsync(InPath, std::move(Snapshot));
}
} // namespace Hyperion
