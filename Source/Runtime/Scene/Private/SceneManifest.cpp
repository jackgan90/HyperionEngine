#include "Hyperion/Scene/SceneManifest.h"
#include <set>

namespace Hyperion
{
void ValidateSceneManifest(const FSceneManifest& InManifest)
{
	std::set<std::string> Assets;
	std::set<std::string> Instances;
	for (const auto& Asset : InManifest.Assets)
	{
		if (Asset.Id.empty() || Asset.Path.empty() || !Assets.insert(Asset.Id).second)
		{
			throw std::invalid_argument("Scene asset IDs and paths must be nonempty; asset IDs must be unique");
		}
	}
	for (const auto& Instance : InManifest.Instances)
	{
		const auto Q = Instance.Rotation;
		const double Norm = double(Q.X) * Q.X + double(Q.Y) * Q.Y + double(Q.Z) * Q.Z + double(Q.W) * Q.W;
		if (Instance.Id.empty() || !Instances.insert(Instance.Id).second || !Assets.contains(Instance.Asset) ||
		    !IsFinite(Instance.Translation) || !IsFinite(Instance.Scale) || !std::isfinite(Norm) ||
		    std::abs(Norm - 1) > .001)
		{
			throw std::invalid_argument("Invalid scene instance ID, asset reference or TRS");
		}
	}
	const auto Direction = Subtract(InManifest.Target, InManifest.Eye);
	if (!IsFinite(InManifest.Eye) || !IsFinite(InManifest.Target) || !IsFinite(Direction) ||
	    !std::isfinite(Length(Direction)) || Length(Direction) < .001f || Length(Cross(Direction, {0, 1, 0})) < .001f ||
	    !std::isfinite(InManifest.Near) || !std::isfinite(InManifest.Far) || InManifest.Near <= 0 ||
	    InManifest.Far <= InManifest.Near)
	{
		throw std::invalid_argument("Invalid scene camera");
	}
}

template<> const FRecordDescriptor& RecordType<FSceneAssetEntry>()
{
	static const auto Type = MakeRecord<FSceneAssetEntry>(
	    "hyperion.sceneasset", {Member("id", &FSceneAssetEntry::Id), Member("path", &FSceneAssetEntry::Path)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneInstanceEntry>()
{
	static const auto Type = MakeRecord<FSceneInstanceEntry>(
	    "hyperion.sceneinstance",
	    {Member("id", &FSceneInstanceEntry::Id), Member("asset", &FSceneInstanceEntry::Asset),
	     Member("translation", &FSceneInstanceEntry::Translation), Member("rotation", &FSceneInstanceEntry::Rotation),
	     Member("scale", &FSceneInstanceEntry::Scale), Member("visible", &FSceneInstanceEntry::bVisible)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FSceneManifest>()
{
	static const auto Type = MakeRecord<FSceneManifest>(
	    "hyperion.scene",
	    {Member("assets", &FSceneManifest::Assets), Member("instances", &FSceneManifest::Instances),
	     Member("eye", &FSceneManifest::Eye), Member("target", &FSceneManifest::Target),
	     Member("near", &FSceneManifest::Near), Member("far", &FSceneManifest::Far)},
	    1, ValidateSceneManifest);
	return Type;
}
} // namespace Hyperion
