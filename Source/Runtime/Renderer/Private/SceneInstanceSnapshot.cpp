#include "SceneInstanceInternal.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
namespace
{
void RebaseReference(FAssetRef& InReference, FAssetService& InAssets, const std::filesystem::path& InSource,
                     const std::filesystem::path& InDestination)
{
	const auto Target = InAssets.Resolve(InReference, InSource);
	const auto Relative = Target.lexically_relative(InDestination.parent_path());
	const auto Text = (Relative.empty() ? Target : Relative).generic_u8string();
	InReference.Path.assign(reinterpret_cast<const char*>(Text.data()), Text.size());
}

std::string SelectionId(const FScene& InScene, std::optional<FSceneHandle> InSelection)
{
	return InSelection ? InScene.FindNode(*InSelection)->Id : std::string{};
}
} // namespace

FSceneManifest FSceneInstance::Snapshot(const std::filesystem::path& InDestination) const
{
	const auto& P = *Impl;
	P.RequireOpen();
	if (!P.Status.bLoaded || !P.Status.Error.empty())
	{
		throw std::runtime_error("Cannot save an unloaded or invalid scene");
	}
	FSceneManifest Result;
	const auto& Settings = P.Scene.GetSettings();
	Result.DefaultCamera = SelectionId(P.Scene, Settings.DefaultCamera);
	Result.MainDirectionalLight = SelectionId(P.Scene, Settings.MainDirectionalLight);
	Result.EnvironmentLight = SelectionId(P.Scene, Settings.EnvironmentLight);
	std::set<std::string> Used;
	const auto Destination = std::filesystem::absolute(InDestination).lexically_normal();
	for (const auto Handle : P.Scene.GetNodes())
	{
		const auto& Node = *P.Scene.FindNode(Handle);
		auto Entry = SceneEntryFromNode(Node);
		if (Node.Model)
		{
			if (Node.Model->Asset.empty())
			{
				throw std::runtime_error("Cannot persist source-less model: " + Node.Name);
			}
			const auto Pending = P.PendingMaterials.find(Handle);
			if (Pending != P.PendingMaterials.end() && Pending->second.bHasStoredSelection)
			{
				throw std::runtime_error("Cannot save pending or failed material selection: " + Node.Id);
			}
			Entry.Model->Surface = PersistSceneMaterialSelection(Node.Model->Surface);
			for (const auto& [Section, Selection] : Node.Model->SectionSurfaces)
			{
				Entry.Model->SectionSurfaces.push_back({Section, PersistSceneMaterialSelection(Selection)});
			}
			Used.insert(Node.Model->Asset);
		}
		Result.Nodes.push_back(std::move(Entry));
	}
	for (const auto& Id : Used)
	{
		if (!P.Manifest)
		{
			throw std::logic_error("Missing asset manifest");
		}
		const auto Entry = std::find_if(P.Manifest->Assets.begin(), P.Manifest->Assets.end(),
		                                [&](const FSceneAssetEntry& InEntry)
		                                {
			                                return InEntry.Id == Id;
		                                });
		if (Entry == P.Manifest->Assets.end())
		{
			throw std::logic_error("Missing scene asset reference");
		}
		Result.Assets.push_back(*Entry);
	}
	// Visit the complete v4 snapshot, including optional model payload material/texture dependencies.
	VisitRecord(RecordType<FSceneManifest>(), &Result,
	            [&](const FRecordDescriptor& InType, const void* InValue, std::string_view)
	            {
		            if (InType.CppType == typeid(FAssetRef))
		            {
			            auto& Reference = *const_cast<FAssetRef*>(static_cast<const FAssetRef*>(InValue));
			            RebaseReference(Reference, P.Assets, P.Path, Destination);
		            }
	            });
	ValidateSceneManifest(Result);
	return Result;
}
} // namespace Hyperion
