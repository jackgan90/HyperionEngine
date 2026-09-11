#pragma once
#include "Hyperion/AssetTypes/AssetTypes.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
struct FSceneAssetEntry
{
	std::string Id;
	FAssetRef Reference;
};

struct FSceneInstanceEntry
{
	std::string Id;
	std::string Asset;
	FMat4 Transform = Identity();
	bool bVisible = true;
	std::string Name;
	FMaterialOverride Material;
};

struct FSceneManifest
{
	std::vector<FSceneAssetEntry> Assets;
	std::vector<FSceneInstanceEntry> Instances;
	FVec3 Eye{0, 3, 12};
	FVec3 Target;
	float Near = .01f;
	float Far = 1000;
};

void ValidateSceneManifest(const FSceneManifest& InManifest);
void RegisterSceneAssetTypes(FRecordRegistry& InRegistry);
template<> const FRecordDescriptor& RecordType<FSceneAssetEntry>();
template<> const FRecordDescriptor& RecordType<FSceneInstanceEntry>();
template<> const FRecordDescriptor& RecordType<FSceneManifest>();
} // namespace Hyperion
