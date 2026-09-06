#pragma once
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/RHI/RHIResources.h"
#include "Hyperion/Shaders/ShaderCompiler.h"
#include <optional>
#include <span>

namespace Hyperion
{
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
	bool AlphaBlend{};
	bool Textured{};
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
	bool Clear{};
	FVec4 ClearColor;
	std::vector<FDrawPacket> Draws;
};

struct FDeviceStats
{
	std::string Adapter;
	bool DebugLayer{};
	std::uint64_t ValidationErrors{};
	std::uint64_t SubmittedFrames{};
	std::uint64_t GpuAllocationBytes{};
};

} // namespace Hyperion
