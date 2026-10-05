#pragma once
#include "Hyperion/Renderer/RenderGraph.h"
#include <type_traits>
#include <variant>

namespace Hyperion
{
struct FGraphPassCommon
{
	std::string Name;
	std::vector<FGraphTexture> Reads;
	std::vector<std::size_t> After;
	FRenderPassTiming Timing;
};

enum class EGraphColorCommandLayout
{
	Single,
	Multiple
};

struct FGraphGraphicsPayload
{
	std::vector<FGraphColorAttachment> Colors;
	std::optional<FGraphDepthStencilAttachment> DepthStencil;
	std::optional<FViewport> Viewport;
	std::vector<FGraphicsDrawBatch> Batches;
	std::function<std::vector<FGraphicsDrawBatch>()> Prepare;
	std::vector<FGraphBufferAccess> Buffers;
	// Preserve the public native command encoding; attachment data has only one accepted list.
	EGraphColorCommandLayout ColorLayout = EGraphColorCommandLayout::Multiple;
};

struct FGraphComputePayload
{
	std::vector<FGraphTextureWrite> Writes;
	std::vector<FGraphBufferAccess> Buffers;
	std::vector<FDispatchPacket> Dispatches;
	std::function<std::vector<FDispatchPacket>()> Prepare;
};

struct FAcceptedGraphPass
{
	FGraphPassCommon Common;
	std::variant<FGraphGraphicsPayload, FGraphComputePayload> Payload;

	bool IsCompute() const
	{
		return std::holds_alternative<FGraphComputePayload>(Payload);
	}

	std::span<const FGraphColorAttachment> GetColors() const
	{
		const auto* Graphics = std::get_if<FGraphGraphicsPayload>(&Payload);
		return Graphics ? std::span<const FGraphColorAttachment>(Graphics->Colors)
		                : std::span<const FGraphColorAttachment>{};
	}

	const std::optional<FGraphDepthStencilAttachment>& GetDepthStencil() const
	{
		static const std::optional<FGraphDepthStencilAttachment> Empty;
		const auto* Graphics = std::get_if<FGraphGraphicsPayload>(&Payload);
		return Graphics ? Graphics->DepthStencil : Empty;
	}

	const std::optional<FViewport>& GetViewport() const
	{
		static const std::optional<FViewport> Empty;
		const auto* Graphics = std::get_if<FGraphGraphicsPayload>(&Payload);
		return Graphics ? Graphics->Viewport : Empty;
	}

	std::span<const FGraphTextureWrite> GetComputeWrites() const
	{
		const auto* Compute = std::get_if<FGraphComputePayload>(&Payload);
		return Compute ? std::span<const FGraphTextureWrite>(Compute->Writes) : std::span<const FGraphTextureWrite>{};
	}

	std::span<const FGraphBufferAccess> GetBuffers() const
	{
		return std::visit(
		    [](const auto& InPayload)
		    {
			    return std::span<const FGraphBufferAccess>(InPayload.Buffers);
		    },
		    Payload);
	}

	bool HasMixedPreparation() const
	{
		return std::visit(
		    [](const auto& InPayload)
		    {
			    if constexpr (std::is_same_v<std::decay_t<decltype(InPayload)>, FGraphGraphicsPayload>)
			    {
				    return InPayload.Prepare && !InPayload.Batches.empty();
			    }
			    else
			    {
				    return InPayload.Prepare && !InPayload.Dispatches.empty();
			    }
		    },
		    Payload);
	}
};

FAcceptedGraphPass AcceptGraphPass(FGraphicsPass InPass);
FAcceptedGraphPass AcceptGraphPass(FComputePass InPass);
} // namespace Hyperion
