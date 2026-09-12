#pragma once
#include "Hyperion/Scene/Model.h"

namespace Hyperion
{
// Embedded source representation for offline import and explicit version-1 upgrade.
enum class EAlphaMode
{
	Opaque = 0,
	Mask = 1,
	Blend = 2
};
enum class EWrapMode
{
	Repeat = 0,
	Clamp = 1,
	Mirror = 2
};
enum class ESamplerFilter
{
	Nearest = 0,
	Linear = 1,
	NearestMipNearest = 2,
	LinearMipNearest = 3,
	NearestMipLinear = 4,
	LinearMipLinear = 5
};

template<> std::span<const EAlphaMode> RecordEnumValues<EAlphaMode>();
template<> std::span<const EWrapMode> RecordEnumValues<EWrapMode>();
template<> std::span<const ESamplerFilter> RecordEnumValues<ESamplerFilter>();

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
	// Tooling-only canonical external image source; embedded images use their owner and index.
	std::string Source;
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
	bool bDoubleSided{};
	bool bUnlit{};
	FTextureBinding BaseColorTexture;
	FTextureBinding MetallicRoughnessTexture;
	FTextureBinding NormalTexture;
	FTextureBinding OcclusionTexture;
	FTextureBinding EmissiveTexture;
};

struct FModelSource
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

void ValidateModelSource(const FModelSource& InModel);
template<> const FRecordDescriptor& RecordType<FModelSampler>();
template<> const FRecordDescriptor& RecordType<FTextureBinding>();
template<> const FRecordDescriptor& RecordType<FModelImage>();
template<> const FRecordDescriptor& RecordType<FModelMaterial>();
template<> const FRecordDescriptor& RecordType<FModelSource>();
} // namespace Hyperion
