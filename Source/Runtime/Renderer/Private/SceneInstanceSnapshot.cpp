#include "Hyperion/IO/Path.h"
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
	InReference.Revision.clear();
	const auto Relative = Target.lexically_relative(InDestination.parent_path());
	if (!IsPackagePath(Target) && Relative.empty())
	{
		throw std::runtime_error("Scene reference cannot be made portable; configure content mounts");
	}
	const auto Text = (IsPackagePath(Target) ? Target : Relative).generic_u8string();
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
	Result.InitialView = Settings.InitialView;
	std::set<std::string> Used;
	const auto Destination = P.Assets.NormalizePath(InDestination);
	for (const auto Handle : P.Scene.GetNodes())
	{
		const auto& Node = *P.Scene.FindNode(Handle);
		auto Entry = SceneEntryFromNode(Node);
		if (Node.Model())
		{
			if (Node.Model()->Asset.empty())
			{
				throw std::runtime_error("Cannot persist source-less model: " + Node.Name);
			}
			const auto Pending = P.PendingMaterials.find(Handle);
			if (Pending != P.PendingMaterials.end() && Pending->second.bHasStoredSelection)
			{
				throw std::runtime_error("Cannot save pending or failed material selection: " + Node.Id);
			}
			Entry.Model->Surface = PersistSceneMaterialSelection(Node.Model()->Surface);
			for (const auto& [Section, Selection] : Node.Model()->SectionSurfaces)
			{
				Entry.Model->SectionSurfaces.push_back({Section, PersistSceneMaterialSelection(Selection)});
			}
			Used.insert(Node.Model()->Asset);
		}
		Result.Nodes.push_back(std::move(Entry));
	}
	for (const auto& Id : Used)
	{
		const auto Entry = P.Loads.find(Id);
		if (Entry == P.Loads.end())
		{
			throw std::logic_error("Missing scene asset reference");
		}
		Result.Assets.push_back({Id, Entry->second.Reference});
	}
	// Visit all component state, including custom component and material/texture dependencies.
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
