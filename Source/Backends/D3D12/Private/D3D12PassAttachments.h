#pragma once
#include "D3D12Resources.h"

namespace Hyperion
{
struct FD3D12FrameTargets
{
	ID3D12Resource* Backbuffer{};
	ID3D12Resource* Depth{};
	D3D12_CPU_DESCRIPTOR_HANDLE ColorView{};
	D3D12_CPU_DESCRIPTOR_HANDLE SrgbView{};
	D3D12_CPU_DESCRIPTOR_HANDLE DepthView{};
	FSize Size;
	ERHIDepthFormat DepthFormat = ERHIDepthFormat::None;
};

FSize ValidatePassAttachments(const FD3D12DeviceState& InState, const FPassCommands& InCommands,
                              const FD3D12FrameTargets& InFrame);
void RecordPassBegin(ID3D12GraphicsCommandList& InList, const FD3D12DeviceState& InState,
                     const FPassCommands& InCommands, const FD3D12FrameTargets& InFrame, FSize InSize);
void RecordPassEnd(ID3D12GraphicsCommandList& InList, const FD3D12DeviceState& InState, const FPassCommands& InCommands,
                   const FD3D12FrameTargets& InFrame, FSize InSize);
} // namespace Hyperion
