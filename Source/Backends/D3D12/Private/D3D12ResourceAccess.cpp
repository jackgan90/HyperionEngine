#include "D3D12Dispatch.h"
#include <algorithm>

namespace Hyperion
{
void ValidateResourceAccesses(const FD3D12DeviceState& InState, const FPassCommands& InCommands)
{
	for (std::size_t Index = 0; Index < InCommands.TextureAccesses.size(); ++Index)
	{
		const auto& Access = InCommands.TextureAccesses[Index];
		const auto& Native = NativeResource<FD3D12Texture>(Access.View.Texture.Payload, &InState);
		const auto Info = Native.GetInfo();
		const bool bWrite = Access.State == EResourceState::ShaderWrite;
		if ((!bWrite && Access.State != EResourceState::ShaderRead) ||
		    (bWrite && (!InCommands.bCompute || !Info.bStorage)) || !Access.View.MipCount ||
		    Access.View.FirstMip >= Info.MipCount || Access.View.MipCount > Info.MipCount - Access.View.FirstMip ||
		    InCommands.GetDepthTexture() == Access.View.Texture ||
		    std::any_of(InCommands.GetColors().begin(), InCommands.GetColors().end(),
		                [&](const auto& InColor)
		                {
			                return InColor.Target.Texture == Access.View.Texture;
		                }))
		{
			throw std::invalid_argument("Invalid texture access range, state or simultaneous attachment");
		}
		for (std::size_t Previous = 0; Previous < Index; ++Previous)
		{
			const auto& Other = InCommands.TextureAccesses[Previous];
			if (Other.View.Texture == Access.View.Texture && Other.State != Access.State &&
			    std::uint64_t(Other.View.FirstMip) + Other.View.MipCount > Access.View.FirstMip &&
			    std::uint64_t(Access.View.FirstMip) + Access.View.MipCount > Other.View.FirstMip)
			{
				throw std::invalid_argument("Overlapping texture SRV/UAV accesses");
			}
		}
	}
	for (std::size_t Index = 0; Index < InCommands.BufferAccesses.size(); ++Index)
	{
		const auto& Access = InCommands.BufferAccesses[Index];
		const auto& Buffer = NativeResource<FD3D12Buffer>(Access.View.Buffer.Payload, &InState);
		const bool bWrite = Access.State == EResourceState::ShaderWrite;
		const auto Usage = bWrite
		                       ? BufferUsage(ERHIBufferUsage::StructuredWrite) | BufferUsage(ERHIBufferUsage::RawWrite)
		                       : BufferUsage(ERHIBufferUsage::StructuredRead) | BufferUsage(ERHIBufferUsage::RawRead);
		// Access declarations describe byte ranges; the binding view separately validates raw/structured layout.
		if ((!bWrite && Access.State != EResourceState::ShaderRead) || (bWrite && !InCommands.bCompute) ||
		    !(Buffer.Usage & Usage) || !Access.View.Size || Access.View.Offset > Buffer.Size ||
		    Access.View.Size > Buffer.Size - Access.View.Offset)
		{
			throw std::invalid_argument("Invalid buffer access range, state or usage");
		}
		for (std::size_t Previous = 0; Previous < Index; ++Previous)
		{
			const auto& Other = InCommands.BufferAccesses[Previous];
			if (Other.View.Buffer == Access.View.Buffer && Other.State != Access.State)
			{
				throw std::invalid_argument("A physical buffer cannot have simultaneous SRV/UAV states");
			}
		}
	}
}
} // namespace Hyperion
