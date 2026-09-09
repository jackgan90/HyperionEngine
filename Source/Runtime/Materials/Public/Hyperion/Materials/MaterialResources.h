#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace Hyperion
{
enum class EMaterialTextureEncoding : std::uint8_t
{
	Linear,
	Srgb
};

// CPU description of a renderer-produced, single-sample depth texture.
struct FMaterialDepthTexture
{
	std::uint32_t Width = 1;
	std::uint32_t Height = 1;
	float ClearDepth = 1;
	bool operator==(const FMaterialDepthTexture&) const = default;
};

struct FMaterialTextureMip
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::vector<std::uint8_t> Bytes;
	bool operator==(const FMaterialTextureMip&) const = default;
};

class FMaterialTextureSource
{
public:
	FMaterialTextureSource(EMaterialTextureEncoding InEncoding, std::vector<FMaterialTextureMip> InMips,
	                       std::uint64_t InVersion = 1);
	explicit FMaterialTextureSource(FMaterialDepthTexture InDepth, std::uint64_t InVersion = 1);
	FMaterialTextureSource(const FMaterialTextureSource&) = delete;
	FMaterialTextureSource& operator=(const FMaterialTextureSource&) = delete;
	std::uint64_t GetIdentity() const;
	std::uint64_t GetVersion() const;
	EMaterialTextureEncoding GetEncoding() const;
	const std::vector<FMaterialTextureMip>& GetMips() const;
	const FMaterialDepthTexture* GetDepthTarget() const;

private:
	std::uint64_t Identity;
	std::uint64_t Version;
	EMaterialTextureEncoding Encoding;
	std::vector<FMaterialTextureMip> Mips;
	FMaterialDepthTexture Depth;
	bool bDepthTarget{};
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
