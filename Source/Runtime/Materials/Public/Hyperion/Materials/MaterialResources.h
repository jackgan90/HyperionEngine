#pragma once
#include "Hyperion/Textures/TextureAsset.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Hyperion
{
// CPU description of a renderer-produced, single-sample depth texture.
struct FMaterialDepthTexture
{
	std::uint32_t Width = 1;
	std::uint32_t Height = 1;
	float ClearDepth = 1;
	bool operator==(const FMaterialDepthTexture&) const = default;
};

enum class EMaterialColorFormat : std::uint8_t
{
	Rgba8Unorm,
	Rgba16Float,
	Rgba32Float
};

// CPU-only description; Renderer maps formats to its chosen RHI backend.
struct FMaterialColorTexture
{
	std::uint32_t Width = 1;
	std::uint32_t Height = 1;
	EMaterialColorFormat Format = EMaterialColorFormat::Rgba16Float;
	std::array<float, 4> Clear{};
	bool operator==(const FMaterialColorTexture&) const = default;
};

class FMaterialTextureSource
{
public:
	explicit FMaterialTextureSource(std::shared_ptr<const FTextureAsset> InAsset, std::uint64_t InVersion = 1);
	FMaterialTextureSource(EMaterialTextureEncoding InEncoding, std::vector<FMaterialTextureMip> InMips,
	                       std::uint64_t InVersion = 1);
	explicit FMaterialTextureSource(FMaterialDepthTexture InDepth, std::uint64_t InVersion = 1);
	explicit FMaterialTextureSource(FMaterialColorTexture InColor, std::uint64_t InVersion = 1);
	FMaterialTextureSource(const FMaterialTextureSource&) = delete;
	FMaterialTextureSource& operator=(const FMaterialTextureSource&) = delete;
	std::uint64_t GetIdentity() const;
	std::uint64_t GetVersion() const;
	EMaterialTextureEncoding GetEncoding() const;
	const std::vector<FMaterialTextureMip>& GetMips() const;
	const std::shared_ptr<const FTextureAsset>& GetAsset() const;
	const FMaterialDepthTexture* GetDepthTarget() const;
	const FMaterialColorTexture* GetColorTarget() const;
	bool IsRenderTarget() const;

private:
	std::uint64_t Identity;
	std::uint64_t Version;
	EMaterialTextureEncoding Encoding;
	std::vector<FMaterialTextureMip> Mips;
	std::shared_ptr<const FTextureAsset> Asset;
	FMaterialDepthTexture Depth;
	bool bDepthTarget{};
	FMaterialColorTexture Color;
	bool bColorTarget{};
};

class FMaterialReadBufferSource
{
public:
	explicit FMaterialReadBufferSource(std::span<const std::byte> InBytes, std::uint64_t InVersion = 1);
	FMaterialReadBufferSource(const FMaterialReadBufferSource&) = delete;
	FMaterialReadBufferSource& operator=(const FMaterialReadBufferSource&) = delete;
	std::uint64_t GetIdentity() const;
	std::uint64_t GetVersion() const;
	std::span<const std::byte> GetBytes() const;

private:
	std::uint64_t Identity;
	std::uint64_t Version;
	std::vector<std::byte> Bytes;
};

enum class EMaterialBufferViewKind : std::uint8_t
{
	Structured,
	Raw
};

struct FMaterialBufferView
{
	std::shared_ptr<const FMaterialReadBufferSource> Source;
	EMaterialBufferViewKind Kind = EMaterialBufferViewKind::Raw;
	std::uint64_t Offset{};
	std::uint64_t Size{};
	std::uint32_t Stride{};
	void Validate() const;
	bool operator==(const FMaterialBufferView&) const = default;
};

enum class EMaterialAddressMode : std::uint8_t
{
	Repeat,
	Clamp,
	Mirror,
	Border,
	MirrorOnce
};

enum class EMaterialSamplerCompare : std::uint8_t
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

struct FMaterialSampler
{
	EMaterialAddressMode U = EMaterialAddressMode::Repeat;
	EMaterialAddressMode V = EMaterialAddressMode::Repeat;
	EMaterialAddressMode W = EMaterialAddressMode::Repeat;
	bool bMinLinear = true;
	bool bMagLinear = true;
	bool bMipLinear = true;
	bool bComparison{};
	std::uint32_t MaxAnisotropy = 1;
	float MipLodBias{};
	float MinLod{};
	float MaxLod = 3.402823466e+38F;
	std::array<float, 4> BorderColor{};
	EMaterialSamplerCompare Compare = EMaterialSamplerCompare::LessEqual;
	void Validate() const;
	bool operator==(const FMaterialSampler&) const = default;
};
} // namespace Hyperion
