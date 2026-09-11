#include "SceneInstanceInternal.h"
#include <algorithm>
#include <set>

namespace Hyperion
{
FSceneManifest FSceneInstance::Snapshot(const std::filesystem::path& InDestination) const
{
	const auto& P = *Impl;
	P.RequireOpen();
	if (!P.Status.bLoaded || !P.Status.Error.empty())
	{
		throw std::runtime_error("Cannot save an unloaded or invalid scene");
	}
	FSceneManifest Result = P.Manifest ? *P.Manifest : FSceneManifest{};
	Result.Assets.clear();
	Result.Instances.clear();
	std::set<std::string> Used;
	const auto Destination = std::filesystem::absolute(InDestination).lexically_normal();
	for (const auto& Instance : P.Models)
	{
		const auto* Model = P.Scene.Find(Instance.Handle);
		if (!Model)
		{
			throw std::logic_error("Scene instance handle is stale");
		}
		if (Instance.Asset.empty())
		{
			throw std::runtime_error("Cannot persist source-less model: " + Model->Name);
		}
		if (Model->Surface.Instance || Model->Surface.Snapshot || !Model->Surface.Overrides.empty() ||
		    !Model->SectionSurfaces.empty())
		{
			throw std::runtime_error("Cannot persist generic runtime material selections: " + Model->Name);
		}
		Result.Instances.push_back(
		    {Instance.Id, Instance.Asset, Model->World, Model->bVisible, Model->Name, Model->Material});
		Used.insert(Instance.Asset);
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
		auto Reference = Entry->Reference;
		const auto Target = P.Assets.Resolve(Reference, P.Path);
		const auto Relative = Target.lexically_relative(Destination.parent_path());
		const auto Text = (Relative.empty() ? Target : Relative).generic_u8string();
		Reference.Path.assign(reinterpret_cast<const char*>(Text.data()), Text.size());
		Result.Assets.push_back({Id, std::move(Reference)});
	}
	ValidateSceneManifest(Result);
	return Result;
}
} // namespace Hyperion
