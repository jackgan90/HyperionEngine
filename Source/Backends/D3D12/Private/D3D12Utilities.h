#pragma once
#include "Hyperion/RHI/RHITypes.h"
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <sstream>
#include <stdexcept>
#include <wrl/client.h>

namespace Hyperion
{
using Microsoft::WRL::ComPtr;
inline constexpr UINT FrameCount = 2;
inline constexpr UINT ContextCount = 16;
inline constexpr UINT TextureCount = 256;

inline void Check(HRESULT InHr, const char* InWhat)
{
	if (FAILED(InHr))
	{
		std::ostringstream S;
		S << InWhat << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(InHr) << ")";
		throw std::runtime_error(S.str());
	}
}

inline std::string Utf8(const wchar_t* InValue)
{
	auto Size = WideCharToMultiByte(CP_UTF8, 0, InValue, -1, nullptr, 0, nullptr, nullptr);
	std::string R(static_cast<std::size_t>(Size), 0);
	WideCharToMultiByte(CP_UTF8, 0, InValue, -1, R.data(), Size, nullptr, nullptr);
	R.pop_back();
	return R;
}

inline D3D12_RESOURCE_DESC BufferDesc(std::uint64_t InSize)
{
	D3D12_RESOURCE_DESC D{};
	D.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	D.Width = InSize;
	D.Height = 1;
	D.DepthOrArraySize = 1;
	D.MipLevels = 1;
	D.SampleDesc.Count = 1;
	D.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	return D;
}

inline D3D12_RESOURCE_STATES Native(EResourceState InState)
{
	return InState == EResourceState::Present ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_RENDER_TARGET;
}

inline void Transition(ID3D12GraphicsCommandList* InList, ID3D12Resource* InResource, D3D12_RESOURCE_STATES InFrom,
                       D3D12_RESOURCE_STATES InTo)
{
	if (InFrom == InTo)
	{
		return;
	}
	D3D12_RESOURCE_BARRIER B{};
	B.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	B.Transition = {InResource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, InFrom, InTo};
	InList->ResourceBarrier(1, &B);
}

} // namespace Hyperion
