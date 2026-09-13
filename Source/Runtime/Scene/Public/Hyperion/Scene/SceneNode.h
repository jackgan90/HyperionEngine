#pragma once
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/SceneCamera.h"
#include "Hyperion/Scene/SceneLight.h"
#include <compare>

namespace Hyperion
{
// Prepared once on a loader/Worker, shared by independently transformed instances.
struct FSceneModelData
{
	std::shared_ptr<const FModelAsset> Asset;
	std::vector<FModelInstance> Instances;
	std::vector<FBounds> PrimitiveBounds;
	FBounds Bounds;
	std::vector<std::shared_ptr<const FMaterialAssetData>> Materials;
	// Optional renderer-prepared defaults belong to this resolved dependency revision, independently of geometry.
	std::vector<std::shared_ptr<const FMaterialSnapshot>> MaterialSnapshots;
};

std::shared_ptr<const FSceneModelData> PrepareSceneModel(
    std::shared_ptr<const FModelAsset> InAsset,
    std::vector<std::shared_ptr<const FMaterialAssetData>> InMaterials = {});
void ValidateMaterialOverride(const FMaterialOverride& InMaterial);

struct FSceneHandle
{
	std::uint64_t Scene{};
	std::uint32_t Slot{};
	std::uint64_t Generation{};
	auto operator<=>(const FSceneHandle&) const = default;
};

struct FSceneMaterialSelection
{
	// Choose either a Main-owned editable instance or an already frozen snapshot; empty means inherited.
	std::shared_ptr<FMaterialInstance> Instance;
	std::shared_ptr<const FMaterialSnapshot> Snapshot;
	FMaterialParameterValues Overrides;
	std::optional<FAssetRef> Reference;
	std::shared_ptr<const FMaterialSnapshot> AssetSnapshot;
	std::map<FAssetRef, std::shared_ptr<const FTextureAsset>> TextureAssets;
	bool operator==(const FSceneMaterialSelection&) const = default;
};

struct FSceneModel
{
	std::string Name;
	std::shared_ptr<const FSceneModelData> Data;
	FMat4 World = Identity();
	bool bVisible = true;
	FMaterialOverride Material;
	FSceneMaterialSelection Surface;
	std::map<std::uint32_t, FSceneMaterialSelection> SectionSurfaces;
};

void ValidateSceneMaterialSelections(const FSceneModel& InModel);

enum class ESceneNodeKind : std::uint8_t
{
	Group,
	Model,
	Camera,
	DirectionalLight,
	EnvironmentLight,
	PointLight,
	SpotLight
};

struct FSceneModelComponent
{
	std::string Asset;
	std::shared_ptr<const FSceneModelData> Data;
	bool bVisible = true;
	FMaterialOverride Material;
	FSceneMaterialSelection Surface;
	std::map<std::uint32_t, FSceneMaterialSelection> SectionSurfaces;
	bool operator==(const FSceneModelComponent& InOther) const;
};

struct FSceneNode
{
	std::string Id;
	std::string Name;
	std::string Parent;
	FMat4 Local = Identity();
	bool bEnabled = true;
	std::optional<FSceneModelComponent> Model;
	std::optional<FSceneCamera> Camera;
	std::optional<FSceneDirectionalLight> DirectionalLight;
	std::optional<FSceneEnvironmentLight> EnvironmentLight;
	std::optional<FScenePointLight> PointLight;
	std::optional<FSceneSpotLight> SpotLight;
	ESceneNodeKind GetKind() const;
	bool operator==(const FSceneNode& InOther) const;
};

struct FSceneNodeView
{
	FSceneHandle Handle;
	const FSceneNode* Node{};
	FMat4 World = Identity();
	bool bEffectiveEnabled{};
};

struct FSceneSettings
{
	std::optional<FSceneHandle> DefaultCamera;
	std::optional<FSceneHandle> MainDirectionalLight;
	std::optional<FSceneHandle> EnvironmentLight;
	bool operator==(const FSceneSettings&) const = default;
};

enum class ESceneReparentMode : std::uint8_t
{
	KeepLocal,
	KeepWorld
};
enum class ESceneChangeMask : std::uint32_t
{
	None = 0,
	Structure = 1,
	Transform = 2,
	Enabled = 4,
	Model = 8,
	Camera = 16,
	Light = 32,
	Metadata = 64,
	Settings = 128
};
ESceneChangeMask operator|(ESceneChangeMask InA, ESceneChangeMask InB);
ESceneChangeMask& operator|=(ESceneChangeMask& InA, ESceneChangeMask InB);
bool HasChange(ESceneChangeMask InMask, ESceneChangeMask InFlags);
const char* ToString(ESceneNodeKind InKind);

struct FSceneChange
{
	FSceneHandle Handle;
	std::uint64_t Revision{};
	// Derived compatibility transfer; never independently mutable scene authority.
	std::optional<FSceneModel> Model;
	ESceneChangeMask Mask = ESceneChangeMask::None;
	ESceneNodeKind Kind = ESceneNodeKind::Group;
	std::optional<FSceneNode> Node;
	FMat4 World = Identity();
	bool bEffectiveEnabled{};
	bool bRemoved{};
	std::optional<FSceneSettings> Settings;
};

void ValidateSceneNode(const FSceneNode& InNode);
FSceneModel SceneModelTransfer(const FSceneNode& InNode, const FMat4& InWorld, bool bInEffectiveEnabled);
FSceneModelComponent SceneModelComponent(const FSceneModel& InModel);
FSceneNode MakeSceneCameraNode(std::string InId, FVec3 InEye = {0, 3, 12}, FVec3 InTarget = {},
                               FSceneCamera InCamera = {});
FSceneNode MakeSceneDirectionalLightNode(std::string InId);
FSceneNode MakeSceneEnvironmentLightNode(std::string InId);
FSceneNode MakeScenePointLightNode(std::string InId);
FSceneNode MakeSceneSpotLightNode(std::string InId);
} // namespace Hyperion
