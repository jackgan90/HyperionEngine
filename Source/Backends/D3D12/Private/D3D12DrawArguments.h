#pragma once
#include "D3D12Resources.h"

namespace Hyperion
{
struct FD3D12IndexedDrawArguments
{
	UINT IndexCount;
	UINT InstanceCount;
	UINT FirstIndex;
	INT VertexOffset;
	static constexpr UINT FirstInstance = 0;
};

// Inputs have already passed resource and draw validation. Construction never resolves or retains resources.
inline D3D12_VERTEX_BUFFER_VIEW NativeVertexBufferView(const FD3D12Buffer& InBuffer, std::uint32_t InVertexStride)
{
	return {InBuffer.Resource->GetGPUVirtualAddress(), static_cast<UINT>(InBuffer.Size), InVertexStride};
}

inline D3D12_INDEX_BUFFER_VIEW NativeIndexBufferView(const FD3D12Buffer& InBuffer)
{
	return {InBuffer.Resource->GetGPUVirtualAddress(), static_cast<UINT>(InBuffer.Size), DXGI_FORMAT_R32_UINT};
}

inline UINT NativeStencilReference(const FGraphicsDynamicState& InState)
{
	return InState.StencilReference;
}

// Borrow only during the current draw; a cached command must copy these values.
inline const std::array<float, 4>& NativeBlendConstants(const FGraphicsDynamicState& InState)
{
	return InState.BlendConstants;
}

inline D3D12_RECT NativeScissor(const FRect& InScissor)
{
	return {InScissor.Left, InScissor.Top, InScissor.Right, InScissor.Bottom};
}

inline FD3D12IndexedDrawArguments NativeIndexedDrawArguments(const FDrawPacket& InDraw)
{
	return {InDraw.IndexCount, InDraw.InstanceCount, InDraw.FirstIndex, InDraw.VertexOffset};
}
} // namespace Hyperion
