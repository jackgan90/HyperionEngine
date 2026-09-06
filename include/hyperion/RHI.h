#pragma once
#include "hyperion/Assets.h"
#include "hyperion/Platform.h"
#include "hyperion/ShaderCompiler.h"
#include <memory>
#include <optional>
#include <span>

namespace Hyperion
{
struct FBufferImpl;
struct FTextureImpl;
struct FPipelineImpl;
struct FRecordedListImpl;

struct FBuffer
{
	std::shared_ptr<FBufferImpl> Impl;

	explicit operator bool() const
	{
		return bool(Impl);
	}
};

struct FTexture
{
	std::shared_ptr<FTextureImpl> Impl;

	explicit operator bool() const
	{
		return bool(Impl);
	}
};

struct FPipeline
{
	std::shared_ptr<FPipelineImpl> Impl;

	explicit operator bool() const
	{
		return bool(Impl);
	}
};

struct FRecordedList
{
	std::shared_ptr<FRecordedListImpl> Impl;
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

// Serialize BeginFrame/EndFrame, resource creation and WaitIdle on RHI 0. Record() may run concurrently
// for distinct contexts [0,16). Returned packets own resources until their frame fence completes.
class FRhiDevice
{
public:
	FRhiDevice(FNativeSurface InSurface, FSize InSize, bool InDebug = true);
	~FRhiDevice();
	FRhiDevice(const FRhiDevice&) = delete;
	FRhiDevice& operator=(const FRhiDevice&) = delete;
	FBuffer CreateBuffer(std::span<const std::byte> InBytes);
	FTexture CreateTexture(const FImage& InImage);
	FPipeline CreatePipeline(const FPipelineDesc& InDesc);
	void BeginFrame(FSize InSize);
	FRecordedList Record(std::uint32_t InContext, const FPassCommands& InCommands);
	FImage EndFrame(std::span<const FRecordedList> InLists, bool InVsync, bool InCapture = false);
	void WaitIdle();
	FDeviceStats Statistics() const;

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
