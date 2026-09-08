#pragma once
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/RHI/RHIBindings.h"
#include "Hyperion/RHI/RHIGraphicsState.h"
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
	Mirror,
	Border,
	MirrorOnce
};

struct FSamplerDesc
{
	ERHIAddressMode U = ERHIAddressMode::Repeat;
	ERHIAddressMode V = ERHIAddressMode::Repeat;
	bool bMinLinear = true;
	bool bMagLinear = true;
	bool bMipLinear = true;
	bool bMipmapped = true;
	ERHIAddressMode W = ERHIAddressMode::Repeat;
	float MipLodBias{};
	float MinLod{};
	float MaxLod = 3.402823466e+38F;
	std::uint32_t MaxAnisotropy = 1;
	std::array<float, 4> BorderColor{};
	bool bComparison{};
	bool operator==(const FSamplerDesc&) const = default;
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
	Unorm8x4,
	Float,
	Int,
	Int2,
	Int3,
	Int4,
	Uint,
	Uint2,
	Uint3,
	Uint4
};

struct FVertexAttribute
{
	std::string Semantic;
	std::uint32_t SemanticIndex{};
	EVertexFormat Format;
	std::uint32_t Offset{};
	bool operator==(const FVertexAttribute&) const = default;
};

struct FPipelineDesc
{
	FShaderArtifact Vertex;
	FShaderArtifact Pixel;
	std::vector<FVertexAttribute> Attributes;
	FResourceBindingLayout Layout;
	FGraphicsState State;
	FGraphicsTarget Target;
	ERHIPrimitiveTopology Topology = ERHIPrimitiveTopology::TriangleList;
	std::uint32_t VertexStride{};
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
	std::uint32_t VertexStride{};
	std::uint32_t IndexCount{};
	std::uint32_t FirstIndex{};
	std::int32_t VertexOffset{};
	FRect Scissor;
	FResourceBindingSet Bindings;
	std::vector<FConstantBinding> ConstantBindings;
	FGraphicsDynamicState DynamicState;
	std::uint32_t InstanceCount = 1;
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
	bool bUseStencil{};
	bool bClearStencil{};
	float ClearDepth = 1;
	std::uint8_t ClearStencil{};
	ERHIDepthFormat DepthFormat = ERHIDepthFormat::D32;
	std::optional<FViewport> Viewport;
	// View family builders set a new domain at each view boundary. Zero is the legacy full target domain.
	std::uint64_t DepthDomain{};
};

struct FDeviceStats
{
	std::string Adapter;
	bool bDebugLayer{};
	std::uint64_t ValidationErrors{};
	std::uint64_t SubmittedFrames{};
	std::uint64_t GpuAllocationBytes{};
	std::uint64_t DescriptorAllocations{};
	std::uint64_t DescriptorCopies{};
	std::uint64_t BindingSetsCreated{};
	std::uint64_t GraphicsRootBinds{};
	std::uint64_t GraphicsHeapBinds{};
	std::uint64_t GraphicsConstantBinds{};
	std::uint64_t GraphicsTableBinds{};
	std::uint64_t PipelinesCreated{};
	std::uint64_t ConstantBytesWritten{};
};

} // namespace Hyperion
