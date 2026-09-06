#pragma once
#include "Hyperion/Math/Math.h"
#include "Hyperion/Reflection/Record.h"
#include <span>

namespace Hyperion
{
enum class EAlphaMode
{
	Opaque,
	Mask,
	Blend
};
enum class EWrapMode
{
	Repeat,
	Clamp,
	Mirror
};
enum class ESamplerFilter
{
	Nearest,
	Linear,
	NearestMipNearest,
	LinearMipNearest,
	NearestMipLinear,
	LinearMipLinear
};

struct FModelSampler
{
	EWrapMode WrapU = EWrapMode::Repeat;
	EWrapMode WrapV = EWrapMode::Repeat;
	ESamplerFilter Min = ESamplerFilter::LinearMipLinear;
	ESamplerFilter Mag = ESamplerFilter::Linear;
};

struct FTextureBinding
{
	std::int32_t Image = -1;
	std::int32_t Sampler = -1;
	std::uint32_t TexCoord{};
};

struct FModelImage
{
	std::string Name;
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::vector<std::uint8_t> Rgba;
};

struct FModelMaterial
{
	std::string Name;
	FVec4 BaseColor{1, 1, 1, 1};
	FVec3 Emissive;
	float Metallic = 1;
	float Roughness = 1;
	float NormalScale = 1;
	float OcclusionStrength = 1;
	float AlphaCutoff = .5f;
	EAlphaMode AlphaMode = EAlphaMode::Opaque;
	bool DoubleSided{};
	bool Unlit{};
	FTextureBinding BaseColorTexture;
	FTextureBinding MetallicRoughnessTexture;
	FTextureBinding NormalTexture;
	FTextureBinding OcclusionTexture;
	FTextureBinding EmissiveTexture;
};

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
	std::vector<FModelMaterial> Materials;
	std::vector<FModelImage> Images;
	std::vector<FModelSampler> Samplers;
	std::vector<FModelNode> Nodes;
	std::vector<std::uint32_t> Roots;
	std::vector<std::string> Diagnostics;
};

struct FModelInstance
{
	std::uint32_t Primitive{};
	FMat4 World;
};

struct FBounds
{
	FVec3 Minimum;
	FVec3 Maximum;
	bool Valid{};
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
template<> const FRecordDescriptor& RecordType<FModelSampler>();
template<> const FRecordDescriptor& RecordType<FTextureBinding>();
template<> const FRecordDescriptor& RecordType<FModelImage>();
template<> const FRecordDescriptor& RecordType<FModelMaterial>();
template<> const FRecordDescriptor& RecordType<FModelPrimitive>();
template<> const FRecordDescriptor& RecordType<FModelNode>();
template<> const FRecordDescriptor& RecordType<FModelAsset>();
} // namespace Hyperion
