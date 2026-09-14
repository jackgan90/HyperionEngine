#pragma once
#include "Hyperion/RHI/RHIDevice.h"
#include "Hyperion/Tasks/TaskSystem.h"
#include <functional>

namespace Hyperion
{
struct FGraphTexture
{
	std::uint64_t Graph{};
	std::size_t Index{};
	bool operator==(const FGraphTexture&) const = default;
};

struct FGraphTextureImport
{
	std::string Name;
	FRenderTarget Target;
	FSize Size;
	ERHIDepthFormat DepthFormat = ERHIDepthFormat::None;
	EResourceState InitialState = EResourceState::ShaderRead;
	bool bInitialized{};
	// A deferred import declares a stable engine source identity before RHI resolution.
	std::shared_ptr<const void> Identity;
	std::function<FTexture()> Resolve;
	ERHIColorFormat ColorFormat = ERHIColorFormat::Rgba8Unorm;
	std::uint32_t MipLevel{}; // Each handle names one independently tracked mip; Size is that mip's extent.
	bool bStorage{};
	bool bSampledOnly{};
};

struct FGraphBuffer
{
	std::uint64_t Graph{};
	std::size_t Index{};
	bool operator==(const FGraphBuffer&) const = default;
};

struct FGraphBufferImport
{
	std::string Name;
	FBuffer Buffer;
	std::uint64_t Size{};
	std::uint32_t Usage{};
	bool bInitialized{};
	EResourceState InitialState = EResourceState::ShaderRead;
	std::shared_ptr<const void> Identity;
	std::function<FBuffer()> Resolve;
};

struct FGraphTextureWrite
{
	FGraphTexture Texture;
	bool bFullOverwrite{}; // False requires preserved initialized contents.
};

struct FGraphBufferAccess
{
	FGraphBuffer Buffer;
	EResourceState State = EResourceState::ShaderRead;
	bool bFullOverwrite{}; // Explicit promise covering the complete buffer.
};

struct FComputePass
{
	std::string Name;
	std::vector<FGraphTexture> Reads;
	std::vector<FGraphTextureWrite> Writes;
	std::vector<FGraphBufferAccess> Buffers;
	std::vector<std::size_t> After;
	std::vector<FDispatchPacket> Dispatches;
	std::function<std::vector<FDispatchPacket>()> Prepare;
};

enum class EGraphColorView
{
	Linear,
	Srgb,
	DrawBatch
};

struct FGraphColorAttachment
{
	FGraphTexture Texture;
	FAttachmentActions Actions;
	FVec4 Clear;
	EGraphColorView View = EGraphColorView::Linear;
};

struct FGraphDepthStencilAttachment
{
	FGraphTexture Texture;
	std::optional<FAttachmentActions> Depth;
	std::optional<FAttachmentActions> Stencil;
	float ClearDepth = 1;
	std::uint8_t ClearStencil{};
};

struct FGraphicsDrawBatch
{
	FDrawCommands Commands;
	bool bSrgb{};
};

struct FGraphicsPass
{
	std::string Name;
	std::optional<FGraphColorAttachment> Color;
	std::optional<FGraphDepthStencilAttachment> DepthStencil;
	std::vector<FGraphTexture> Reads;
	std::optional<FViewport> Viewport;
	std::vector<std::size_t> After;
	std::vector<FGraphicsDrawBatch> Batches;
	// Only draw batches are deferred. Attachments and resource accesses are already declared.
	std::function<std::vector<FGraphicsDrawBatch>()> Prepare;
	std::vector<FGraphColorAttachment> Colors;
	// Internal common graph storage; clients declare computation with FComputePass.
	bool bCompute{};
	std::vector<FGraphTextureWrite> ComputeWrites;
	std::vector<FGraphBufferAccess> Buffers;
	std::vector<FDispatchPacket> Dispatches;
	std::function<std::vector<FDispatchPacket>()> PrepareCompute;

	std::span<const FGraphColorAttachment> GetColors() const
	{
		if (Color && !Colors.empty())
		{
			throw std::invalid_argument("Graph pass cannot mix single and multiple color attachment storage");
		}
		return Color ? std::span<const FGraphColorAttachment>(&*Color, 1)
		             : std::span<const FGraphColorAttachment>(Colors);
	}
};

class FRenderGraph
{
public:
	FRenderGraph();

	std::uint64_t GetIdentity() const noexcept
	{
		return Identity;
	}

	FRenderGraph(const FRenderGraph& InOther);
	FRenderGraph(FRenderGraph&& InOther);
	FRenderGraph& operator=(const FRenderGraph& InOther);
	FRenderGraph& operator=(FRenderGraph&& InOther);
	FGraphTexture Import(FGraphTextureImport InResource);
	FGraphBuffer Import(FGraphBufferImport InResource);
	FGraphTexture ImportBackbuffer(FSize InSize = {});
	FGraphTexture ImportFrameDepth(ERHIDepthFormat InFormat, FSize InSize = {});
	void Export(FGraphTexture InTexture, EResourceState InState);
	void Export(FGraphBuffer InBuffer, EResourceState InState);
	std::size_t Add(FGraphicsPass InPass);
	std::size_t AddCompute(FComputePass InPass);
	std::vector<FPassCommands> Compile() const;
	std::vector<FPassCommands> CompileAndConsume();

private:
	bool bCompiling{};
	void CheckMutable() const;
	std::uint64_t Identity;
	std::vector<FGraphTextureImport> Resources;
	std::vector<std::pair<FGraphTexture, EResourceState>> Exports;
	std::vector<FGraphicsPass> Passes;
	std::vector<FGraphBufferImport> Buffers;
	std::vector<std::pair<FGraphBuffer, EResourceState>> BufferExports;
	std::size_t ResourceIndex(FGraphTexture InTexture) const;
	std::vector<std::size_t> Order() const;
	void Reset();
};

struct FRenderGraphCallbacks
{
	std::function<void()> BeforePrepare;
	std::function<void()> AfterSubmit;
	std::function<void()> OnFailure;
};

// Called on RHI 0 by an owned frame job; completes all recording peers before returning.
FImage ExecuteGraphOnRhi(FRenderGraph InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                         bool bInVsync, bool bInCapture, const FRenderGraphCallbacks& InCallbacks = {});

// Called from Render; jobs are routed to engine-owned RHI executors.
FImage ExecuteGraph(const FRenderGraph& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture);
// Consuming execution can collect coordinator-only statistics in the successful EndFrame task.
FImage ExecuteGraph(FRenderGraph&& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture, const std::function<void()>& InAfterSubmit = {});
FImage ExecuteGraph(FRenderGraph&& InGraph, FTaskSystem& InTasks, IRHISwapchain& InSwapchain, FSize InSize,
                    bool bInVsync, bool bInCapture, const FRenderGraphCallbacks& InCallbacks);
} // namespace Hyperion
