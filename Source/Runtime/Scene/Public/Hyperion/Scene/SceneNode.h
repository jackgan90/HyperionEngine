#pragma once
#include "Hyperion/Materials/MaterialAsset.h"
#include "Hyperion/Scene/SceneCamera.h"
#include "Hyperion/Scene/SceneComponent.h"
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
	std::map<std::string, std::vector<FModelInstance>, std::less<>> NodeInstances;
	std::map<std::string, FBounds, std::less<>> NodeBounds;
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
	std::string SourceNode;
	std::vector<FSceneMeshSection> Sections;
	std::string SourcePrimitive;
};

void ValidateSceneMaterialSelections(const FSceneModel& InModel);
FBounds SceneModelBounds(const FSceneModel& InModel);
std::vector<FModelInstance> SceneModelInstances(const FSceneModel& InModel);

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
	std::string SourceNode;
	std::vector<FSceneMeshSection> Sections;
	std::string SourcePrimitive;
	bool operator==(const FSceneModelComponent& InOther) const;
};

struct FSceneTransform
{
	std::string Parent;
	FMat4 Local = Identity();

	bool operator==(const FSceneTransform& InOther) const
	{
		return Parent == InOther.Parent && Local.Values == InOther.Local.Values;
	}
};

struct FSceneModelSource
{
	std::string Asset;
	std::string SourceNode;
	std::string InstanceRoot;
	bool operator==(const FSceneModelSource&) const = default;
};

template<> const FRecordDescriptor& RecordType<FSceneModelSource>();

template<> const FRecordDescriptor& RecordType<FSceneTransform>();
template<> const FRecordDescriptor& RecordType<FSceneModelComponent>();

struct FSceneNode
{
	FSceneNode();
	std::string Id;
	std::string Name;
	bool bEnabled = true;
	FSceneComponents Components;

	const std::string& Parent() const
	{
		return Components.Slot<FSceneTransform>().value().Parent;
	}

	std::string& Parent()
	{
		return Components.Slot<FSceneTransform>().value().Parent;
	}

	const FMat4& Local() const
	{
		return Components.Slot<FSceneTransform>().value().Local;
	}

	FMat4& Local()
	{
		return Components.Slot<FSceneTransform>().value().Local;
	}

	const std::optional<FSceneModelComponent>& Model() const
	{
		return Components.Slot<FSceneModelComponent>();
	}

	std::optional<FSceneModelComponent>& Model()
	{
		return Components.Slot<FSceneModelComponent>();
	}

	const std::optional<FSceneCamera>& Camera() const
	{
		return Components.Slot<FSceneCamera>();
	}

	std::optional<FSceneCamera>& Camera()
	{
		return Components.Slot<FSceneCamera>();
	}

	const std::optional<FSceneDirectionalLight>& DirectionalLight() const
	{
		return Components.Slot<FSceneDirectionalLight>();
	}

	std::optional<FSceneDirectionalLight>& DirectionalLight()
	{
		return Components.Slot<FSceneDirectionalLight>();
	}

	const std::optional<FSceneEnvironmentLight>& EnvironmentLight() const
	{
		return Components.Slot<FSceneEnvironmentLight>();
	}

	std::optional<FSceneEnvironmentLight>& EnvironmentLight()
	{
		return Components.Slot<FSceneEnvironmentLight>();
	}

	const std::optional<FScenePointLight>& PointLight() const
	{
		return Components.Slot<FScenePointLight>();
	}

	std::optional<FScenePointLight>& PointLight()
	{
		return Components.Slot<FScenePointLight>();
	}

	const std::optional<FSceneSpotLight>& SpotLight() const
	{
		return Components.Slot<FSceneSpotLight>();
	}

	std::optional<FSceneSpotLight>& SpotLight()
	{
		return Components.Slot<FSceneSpotLight>();
	}

	bool Has(ESceneNodeKind InKind) const;
	// Compatibility display hint; a node can have more than one capability.
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
	// Explicit browsing preset; independent of authored camera objects and runtime selection.
	std::optional<FSceneCameraView> InitialView;
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
