#pragma once
#include "Hyperion/AssetTypes/AssetTypes.h"
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/SceneNode.h"

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

// Compatibility input only; never the live or current persisted scene.
struct FLegacySceneManifest
{
	std::vector<FSceneAssetEntry> Assets;
	std::vector<FSceneInstanceEntry> Instances;
	FVec3 Eye{0, 3, 12};
	FVec3 Target;
	float Near = .01f;
	float Far = 1000;
};

struct FSceneNodeModel
{
	std::string Asset;
	bool bVisible = true;
	FMaterialOverride Material;
	FSceneMaterialAsset Surface;
	std::vector<FSceneSectionMaterial> SectionSurfaces;
	std::string SourceNode;
	std::vector<FSceneMeshSection> Sections;
	std::string SourcePrimitive;
};

struct FSceneNodeEntry
{
	std::string Id;
	std::string Name;
	std::string Parent;
	FMat4 Transform = Identity();
	bool bEnabled = true;
	std::optional<FSceneNodeModel> Model;
	std::optional<FSceneCamera> Camera;
	std::optional<FSceneDirectionalLight> DirectionalLight;
	std::optional<FSceneEnvironmentLight> EnvironmentLight;
	std::optional<FScenePointLight> PointLight;
	std::optional<FSceneSpotLight> SpotLight;
	// Typed fields are a loading DTO. The canonical archive is one component array.
	std::map<std::string, std::string> ComponentIds;
	FSceneComponents Extensions;
};

struct FSceneManifest
{
	std::vector<FSceneAssetEntry> Assets;
	std::vector<FSceneNodeEntry> Nodes;
	std::string DefaultCamera;
	std::string MainDirectionalLight;
	std::string EnvironmentLight;
	std::optional<FSceneCameraView> InitialView;
};

void ValidateLegacySceneManifest(const FLegacySceneManifest& InManifest);
FSceneManifest UpgradeLegacyScene(const FLegacySceneManifest& InManifest);
FSceneNode NodeFromSceneEntry(const FSceneNodeEntry& InEntry);
FSceneNodeEntry SceneEntryFromNode(const FSceneNode& InNode);
std::vector<FSceneNode> NodesFromSceneManifest(const FSceneManifest& InManifest);
FSceneSettings ResolveSceneSettings(const FSceneManifest& InManifest, const class FScene& InScene);
std::size_t SceneModelCount(const FSceneManifest& InManifest);
// Explicit authoring operation only; importing, upgrading and loading never call this automatically.
void ExpandSceneModels(FSceneManifest& InManifest,
                       const std::map<std::string, std::shared_ptr<const FModelAsset>>& InModels);
template<> const FRecordDescriptor& RecordType<FLegacySceneManifest>();
template<> const FRecordDescriptor& RecordType<FSceneNodeModel>();
template<> const FRecordDescriptor& RecordType<FSceneNodeEntry>();

void ValidateSceneMaterialAsset(const FSceneMaterialAsset& InMaterial);
template<> const FRecordDescriptor& RecordType<FSceneMaterialAsset>();
template<> const FRecordDescriptor& RecordType<FSceneSectionMaterial>();

void ValidateSceneManifest(const FSceneManifest& InManifest);
void RegisterSceneAssetTypes(FRecordRegistry& InRegistry);
template<> const FRecordDescriptor& RecordType<FSceneAssetEntry>();
template<> const FRecordDescriptor& RecordType<FSceneInstanceEntry>();
template<> const FRecordDescriptor& RecordType<FSceneManifest>();
} // namespace Hyperion
