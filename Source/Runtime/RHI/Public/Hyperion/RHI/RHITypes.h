#pragma once
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/RHI/RHIResources.h"
#include "Hyperion/Shaders/ShaderCompiler.h"
#include <optional>
#include <span>

namespace Hyperion
{
enum class ERHIAddressMode
{
	Repeat,
	Clamp,
	Mirror
};

struct FSamplerDesc
{
	ERHIAddressMode U = ERHIAddressMode::Repeat;
	ERHIAddressMode V = ERHIAddressMode::Repeat;
	bool bMinLinear = true;
	bool bMagLinear = true;
	bool bMipLinear = true;
	bool bMipmapped = true;
};

struct FTextureMip
{
	std::uint32_t Width{};
	std::uint32_t Height{};
	std::vector<std::uint8_t> Rgba;
};

struct FTextureDesc
{
	bool bSrgb{};
	std::vector<FTextureMip> Mips;
};
enum class EVertexFormat
{
	Float2,
	Float3,
	Float4,
	Unorm8x4
};

struct FVertexAttribute
{
	std::string Semantic;
	std::uint32_t SemanticIndex{};
	EVertexFormat Format;
	std::uint32_t Offset{};
};

struct FPipelineDesc
{
	FShaderArtifact Vertex;
	FShaderArtifact Pixel;
	std::vector<FVertexAttribute> Attributes;
	bool bAlphaBlend{};
	bool bTextured{};
	bool bMaterialLayout{};
	bool bSrgbTarget{};
	bool bDepthTest{};
	bool bDepthWrite{};
	bool bCullBack{};
	bool bFrontCounterClockwise = true;
	std::array<FSamplerDesc, 5> Samplers;
};

struct FRect
{
	std::int32_t Left{};
	std::int32_t Top{};
	std::int32_t Right{};
	std::int32_t Bottom{};
};

struct FDrawPacket
{
	FPipeline Pipeline;
	FBuffer Vertices;
	FBuffer Indices;
	FTexture Texture;
	std::uint32_t VertexStride{};
	std::uint32_t IndexCount{};
	std::uint32_t FirstIndex{};
	std::int32_t VertexOffset{};
	FMat4 Constants;
	FRect Scissor;
	FBuffer MaterialConstants;
	std::array<FTexture, 5> MaterialTextures;
	// Material layout uses a 512-byte slice at a 256-byte aligned buffer offset.
	std::uint64_t MaterialConstantOffset{};
};
enum class EResourceState
{
	Present,
	RenderTarget
};

struct FPassCommands
{
	std::string Name;
	std::optional<EResourceState> TransitionFrom;
	std::optional<EResourceState> TransitionTo;
	bool bClear{};
	FVec4 ClearColor;
	std::vector<FDrawPacket> Draws;
	bool bUseDepth{};
	bool bSrgbTarget{};
	bool bClearDepth{};
};

struct FDeviceStats
{
	std::string Adapter;
	bool bDebugLayer{};
	std::uint64_t ValidationErrors{};
	std::uint64_t SubmittedFrames{};
	std::uint64_t GpuAllocationBytes{};
};

} // namespace Hyperion
