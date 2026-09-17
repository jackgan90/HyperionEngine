#pragma once
#include "Hyperion/AssetTypes/AssetTypes.h"
#include "Hyperion/Math/Bounds.h"
#include <span>

namespace Hyperion
{
struct FMaterialOverride
{
	std::optional<FVec4> BaseColor;
	std::optional<float> Metallic;
	std::optional<float> Roughness;
	bool operator==(const FMaterialOverride& InOther) const;
};

struct FSceneMeshSection
{
	std::string Primitive;
	bool bVisible = true;
	FMaterialOverride Material;
	bool operator==(const FSceneMeshSection&) const = default;
};

template<> const FRecordDescriptor& RecordType<FSceneMeshSection>();

void ValidateMaterialOverride(const FMaterialOverride& InMaterial);
template<> const FRecordDescriptor& RecordType<FMaterialOverride>();

struct FModelPrimitive
{
	std::string Name;
	std::vector<float> Positions;
	std::vector<float> Normals;
	std::vector<float> Tangents;
	std::vector<float> Colors;
	std::vector<float> TexCoords0;
	std::vector<float> TexCoords1;
	std::vector<std::uint32_t> Indices;
	std::int32_t Material = -1;
	std::string Id;
};

struct FModelNode
{
	std::string Name;
	FMat4 Local = Identity();
	std::vector<std::uint32_t> Primitives;
	std::vector<std::uint32_t> Children;
	std::string Id;
};

struct FModelAsset
{
	std::string Name;
	std::vector<FModelPrimitive> Primitives;
	std::vector<FAssetRef> MaterialSlots;
	std::vector<FModelNode> Nodes;
	std::vector<std::uint32_t> Roots;
	std::vector<std::string> Diagnostics;
};

struct FModelInstance
{
	std::uint32_t Primitive{};
	FMat4 World;
};

void ValidateModel(const FModelAsset& InModel);
void ValidateNodeHierarchy(std::span<const FModelNode> InNodes);
std::vector<FModelInstance> ModelInstances(const FModelAsset& InModel);
std::string ModelNodeId(const FModelAsset& InModel, std::size_t InIndex);
std::string ModelPrimitiveId(const FModelAsset& InModel, std::size_t InIndex);
void AssignModelSubresourceIds(FModelAsset& InModel);
std::vector<FModelInstance> SelectedModelInstances(const FModelAsset& InModel, std::string_view InSourceNode);
FBounds ModelBounds(const FModelAsset& InModel);
void GenerateMeshDirections(FModelPrimitive& InPrimitive, std::uint32_t InTangentUv = 0);

template<> const FRecordDescriptor& RecordType<FVec2>();
template<> const FRecordDescriptor& RecordType<FVec3>();
template<> const FRecordDescriptor& RecordType<FVec4>();
template<> const FRecordDescriptor& RecordType<FMat4>();
template<> const FRecordDescriptor& RecordType<FModelPrimitive>();
template<> const FRecordDescriptor& RecordType<FModelNode>();
template<> const FRecordDescriptor& RecordType<FModelAsset>();
} // namespace Hyperion
