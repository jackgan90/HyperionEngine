#pragma once
#include "D3D12Resources.h"

namespace Hyperion
{
struct FD3D12DrawPlan;

// One weak stream per recording context. Entries never extend resource or constant-page lifetimes.
struct FD3D12DrawCache
{
	std::mutex Mutex;
	std::weak_ptr<const std::vector<FDrawPacket>> Owner;
	std::weak_ptr<IRHITexture> DepthTarget;
	std::vector<std::weak_ptr<IRHITexture>> Reads;

	struct FTextureAccessKey
	{
		std::weak_ptr<IRHITexture> Texture;
		std::uint32_t FirstMip{};
		std::uint32_t MipCount{};
		EResourceState State{};
	};

	std::vector<FTextureAccessKey> TextureAccesses;

	struct FBufferAccessKey
	{
		std::weak_ptr<IRHIBuffer> Buffer;
		std::uint64_t Offset{};
		std::uint64_t Size{};
		EResourceState State{};
	};

	std::vector<FBufferAccessKey> BufferAccesses;
	std::array<unsigned, 7> Target{};
	FGraphicsTarget GraphicsTarget;
	std::vector<std::weak_ptr<IRHITexture>> ColorTargets;
	std::shared_ptr<const FD3D12DrawPlan> Plan;
};

std::shared_ptr<const FD3D12DrawPlan> PrepareNativeDraws(const FD3D12DeviceState& InState,
                                                         const std::shared_ptr<const FPassCommands>& InOwnedCommands,
                                                         FD3D12DrawCache& InCache);
void RecordNativeDrawPlan(ID3D12GraphicsCommandList& InList, const FD3D12DrawPlan& InPlan, FD3D12DeviceState& InState);
void ValidateDraws(const FD3D12DeviceState& InState, const FPassCommands& InCommands);
void RecordDraws(ID3D12GraphicsCommandList& InList, const FPassCommands& InCommands, FD3D12DeviceState& InState);
} // namespace Hyperion
