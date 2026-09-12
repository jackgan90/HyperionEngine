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
};

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
};

struct FModelNode
{
	std::string Name;
	FMat4 Local = Identity();
	std::vector<std::uint32_t> Primitives;
	std::vector<std::uint32_t> Children;
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
