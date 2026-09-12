#pragma once
#include "Hyperion/AssetTypes/AssetTypes.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
struct FSceneAssetEntry
{
	std::string Id;
	FAssetRef Reference;
};

struct FSceneMaterialAsset
{
	std::optional<FAssetRef> Reference;
	FMaterialAssetValues Values;
	FMaterialAssetValues Overrides;
};

struct FSceneSectionMaterial
{
	std::uint32_t Section{};
	FSceneMaterialAsset Material;
};

struct FSceneInstanceEntry
{
	std::string Id;
	std::string Asset;
	FMat4 Transform = Identity();
	bool bVisible = true;
	std::string Name;
	FMaterialOverride Material;
	FSceneMaterialAsset Surface;
	std::vector<FSceneSectionMaterial> SectionSurfaces;
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

void ValidateSceneMaterialAsset(const FSceneMaterialAsset& InMaterial);
template<> const FRecordDescriptor& RecordType<FSceneMaterialAsset>();
template<> const FRecordDescriptor& RecordType<FSceneSectionMaterial>();

void ValidateSceneManifest(const FSceneManifest& InManifest);
void RegisterSceneAssetTypes(FRecordRegistry& InRegistry);
template<> const FRecordDescriptor& RecordType<FSceneAssetEntry>();
template<> const FRecordDescriptor& RecordType<FSceneInstanceEntry>();
template<> const FRecordDescriptor& RecordType<FSceneManifest>();
} // namespace Hyperion
