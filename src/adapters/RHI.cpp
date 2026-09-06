#include "hyperion/RHI.h"
#include "hyperion/Core.h"
#include <D3D12MemAlloc.h>
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <wrl/client.h>

namespace Hyperion
{
using Microsoft::WRL::ComPtr;

namespace
{
constexpr UINT FrameCount = 2;
constexpr UINT ContextCount = 16;
constexpr UINT TextureCount = 256;

void Check(HRESULT InHr, const char* InWhat)
{
	if (FAILED(InHr))
	{
		std::ostringstream S;
		S << InWhat << " (HRESULT 0x" << std::hex << static_cast<unsigned long>(InHr) << ")";
		throw std::runtime_error(S.str());
	}
}

std::string Utf8(const wchar_t* InValue)
{
	auto Size = WideCharToMultiByte(CP_UTF8, 0, InValue, -1, nullptr, 0, nullptr, nullptr);
	std::string R(static_cast<std::size_t>(Size), 0);
	WideCharToMultiByte(CP_UTF8, 0, InValue, -1, R.data(), Size, nullptr, nullptr);
	R.pop_back();
	return R;
}

D3D12_RESOURCE_DESC BufferDesc(std::uint64_t InSize)
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

D3D12_RESOURCE_STATES Native(EResourceState InState)
{
	return InState == EResourceState::Present ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void Transition(ID3D12GraphicsCommandList* InList, ID3D12Resource* InResource, D3D12_RESOURCE_STATES InFrom,
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

struct FDeviceState
{
	ComPtr<ID3D12Device> Device;
	ComPtr<D3D12MA::Allocator> Allocator;
	ComPtr<ID3D12DescriptorHeap> Textures;
	UINT TextureStep{};
	std::mutex DescriptorsMutex;
	std::array<bool, TextureCount> Occupied{};

	UINT Reserve()
	{
		std::lock_guard Lock(DescriptorsMutex);
		for (UINT I = 0; I < TextureCount; ++I)
		{
			if (!Occupied[I])
			{
				Occupied[I] = true;
				return I;
			}
		}
		throw std::runtime_error("Texture descriptor capacity exceeded");
	}

	void Release(UINT InI)
	{
		std::lock_guard Lock(DescriptorsMutex);
		Occupied[InI] = false;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Cpu(UINT InI) const
	{
		auto H = Textures->GetCPUDescriptorHandleForHeapStart();
		H.ptr += std::size_t(InI) * TextureStep;
		return H;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Gpu(UINT InI) const
	{
		auto H = Textures->GetGPUDescriptorHandleForHeapStart();
		H.ptr += std::uint64_t(InI) * TextureStep;
		return H;
	}
};
} // namespace

struct FBufferImpl
{
	std::shared_ptr<FDeviceState> State;
	ComPtr<D3D12MA::Allocation> Allocation;
	ComPtr<ID3D12Resource> Resource;
	std::uint64_t Size{};
};

struct FTextureImpl
{
	std::shared_ptr<FDeviceState> State;
	ComPtr<D3D12MA::Allocation> Allocation;
	ComPtr<ID3D12Resource> Resource;
	UINT Slot = TextureCount;

	~FTextureImpl()
	{
		if (Slot < TextureCount)
		{
			State->Release(Slot);
		}
	}
};

struct FPipelineImpl
{
	ComPtr<ID3D12RootSignature> Root;
	ComPtr<ID3D12PipelineState> Pipeline;
	bool Textured{};
};

struct FRecordedListImpl
{
	ComPtr<ID3D12GraphicsCommandList> List;
	std::vector<FDrawPacket> Retained;
	std::uint64_t Frame{};
	UINT Context{};
	const void* Owner{};
};

struct FRhiDevice::FImpl
{
	std::shared_ptr<FDeviceState> State = std::make_shared<FDeviceState>();
	ComPtr<IDXGIFactory6> Factory;
	ComPtr<IDXGIAdapter1> Adapter;
	ComPtr<IDXGISwapChain3> Swapchain;
	ComPtr<ID3D12CommandQueue> Queue;
	ComPtr<ID3D12Fence> Fence;
	ComPtr<ID3D12InfoQueue> Info;
	ComPtr<ID3D12DescriptorHeap> Rtvs;
	std::array<ComPtr<ID3D12Resource>, FrameCount> Backbuffers;

	struct FFrame
	{
		std::array<ComPtr<ID3D12CommandAllocator>, ContextCount> Allocators;
		std::array<std::atomic<bool>, ContextCount> Recorded{};
		std::vector<FRecordedList> Retained;
		std::uint64_t FenceValue{};
	};

	std::array<FFrame, FrameCount> Frames;
	HANDLE Event{};
	UINT FrameIndex{};
	UINT RtvStep{};
	std::uint64_t NextFence = 1;
	std::uint64_t Serial{};
	std::uint64_t Submitted{};
	FSize Size;
	bool Active{};
	bool Debug{};
	std::string AdapterName;

	~FImpl()
	{
		if (Event)
		{
			CloseHandle(Event);
		}
	}

	void Wait(std::uint64_t InValue)
	{
		if (Fence->GetCompletedValue() < InValue)
		{
			Check(Fence->SetEventOnCompletion(InValue, Event), "Fence event");
			if (WaitForSingleObject(Event, 30000) != WAIT_OBJECT_0)
			{
				throw std::runtime_error("GPU fence timed out or device was removed");
			}
		}
	}

	std::uint64_t Signal()
	{
		auto Value = NextFence++;
		Check(Queue->Signal(Fence.Get(), Value), "Signal GPU fence");
		return Value;
	}

	void Idle()
	{
		Wait(Signal());
		for (auto& F : Frames)
		{
			F.Retained.clear();
		}
	}

	void Buffers()
	{
		for (UINT I = 0; I < FrameCount; ++I)
		{
			Check(Swapchain->GetBuffer(I, IID_PPV_ARGS(&Backbuffers[I])), "Get swapchain buffer");
			auto H = Rtvs->GetCPUDescriptorHandleForHeapStart();
			H.ptr += std::size_t(I) * RtvStep;
			State->Device->CreateRenderTargetView(Backbuffers[I].Get(), nullptr, H);
		}
	}

	FBuffer AllocateBuffer(std::uint64_t InBytes, D3D12_HEAP_TYPE InHeap, D3D12_RESOURCE_STATES InInitial)
	{
		auto R = std::make_shared<FBufferImpl>();
		R->State = State;
		R->Size = InBytes;
		D3D12MA::ALLOCATION_DESC A{};
		A.HeapType = InHeap;
		auto D = BufferDesc(InBytes);
		Check(State->Allocator->CreateResource(&A, &D, InInitial, nullptr, &R->Allocation, IID_PPV_ARGS(&R->Resource)),
		      "Allocate GPU buffer");
		return {std::move(R)};
	}

	void Immediate(const std::function<void(ID3D12GraphicsCommandList*)>& InRecord)
	{
		ComPtr<ID3D12CommandAllocator> A;
		ComPtr<ID3D12GraphicsCommandList> L;
		Check(State->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&A)),
		      "Upload allocator");
		Check(State->Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, A.Get(), nullptr, IID_PPV_ARGS(&L)),
		      "Upload list");
		InRecord(L.Get());
		Check(L->Close(), "Close upload list");
		ID3D12CommandList* Lists[] = {L.Get()};
		Queue->ExecuteCommandLists(1, Lists);
		Wait(Signal());
	}
};

FRhiDevice::FRhiDevice(FNativeSurface InSurface, FSize InSize, bool InDebug) : Impl(std::make_unique<FImpl>())
{
	auto& P = *Impl;
	P.Size = InSize;
	if (!InSurface.Handle || !InSize.Width || !InSize.Height)
	{
		throw std::invalid_argument("Invalid RHI surface");
	}
	if (InDebug)
	{
		ComPtr<ID3D12Debug> Layer;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&Layer))))
		{
			Layer->EnableDebugLayer();
			P.Debug = true;
		}
	}
	Check(CreateDXGIFactory2(P.Debug ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&P.Factory)), "Create DXGI factory");
	for (UINT I = 0;; ++I)
	{
		ComPtr<IDXGIAdapter1> Candidate;
		auto Hr =
		    P.Factory->EnumAdapterByGpuPreference(I, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&Candidate));
		if (Hr == DXGI_ERROR_NOT_FOUND)
		{
			break;
		}
		Check(Hr, "Enumerate adapters");
		DXGI_ADAPTER_DESC1 Desc{};
		Candidate->GetDesc1(&Desc);
		if (Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}
		if (SUCCEEDED(D3D12CreateDevice(Candidate.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&P.State->Device))))
		{
			P.Adapter = Candidate;
			P.AdapterName = Utf8(Desc.Description);
			break;
		}
	}
	if (!P.State->Device)
	{
		throw std::runtime_error("No hardware D3D12 feature-level 12.0 adapter found");
	}
	if (P.Debug)
	{
		P.State->Device.As(&P.Info);
	}
	D3D12MA::ALLOCATION_CALLBACKS Callbacks{};
	Callbacks.pAllocate = [](size_t InSize, size_t InAlignment, void*) -> void*
	{
		try
		{
			return Allocate(InSize, InAlignment, EMemoryTag::Render);
		}
		catch (...)
		{
			return nullptr;
		}
	};
	Callbacks.pFree = [](void* InPtr, void*)
	{
		Deallocate(InPtr);
	};
	D3D12MA::ALLOCATOR_DESC Ma{};
	Ma.pDevice = P.State->Device.Get();
	Ma.pAdapter = P.Adapter.Get();
	Ma.pAllocationCallbacks = &Callbacks;
	Check(D3D12MA::CreateAllocator(&Ma, &P.State->Allocator), "Create GPU allocator");
	D3D12_DESCRIPTOR_HEAP_DESC Heap{};
	Heap.NumDescriptors = TextureCount;
	Heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	Heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	Check(P.State->Device->CreateDescriptorHeap(&Heap, IID_PPV_ARGS(&P.State->Textures)), "Texture descriptor heap");
	P.State->TextureStep = P.State->Device->GetDescriptorHandleIncrementSize(Heap.Type);
	Heap = {};
	Heap.NumDescriptors = FrameCount;
	Heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	Check(P.State->Device->CreateDescriptorHeap(&Heap, IID_PPV_ARGS(&P.Rtvs)), "RTV heap");
	P.RtvStep = P.State->Device->GetDescriptorHandleIncrementSize(Heap.Type);
	D3D12_COMMAND_QUEUE_DESC Q{};
	Q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	Check(P.State->Device->CreateCommandQueue(&Q, IID_PPV_ARGS(&P.Queue)), "Create graphics queue");
	Check(P.State->Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&P.Fence)), "Create GPU fence");
	P.Event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!P.Event)
	{
		throw std::runtime_error("Create fence event failed");
	}
	DXGI_SWAP_CHAIN_DESC1 Sc{};
	Sc.Width = InSize.Width;
	Sc.Height = InSize.Height;
	Sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Sc.SampleDesc.Count = 1;
	Sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Sc.BufferCount = FrameCount;
	Sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	ComPtr<IDXGISwapChain1> Swapchain;
	Check(P.Factory->CreateSwapChainForHwnd(P.Queue.Get(), static_cast<HWND>(InSurface.Handle), &Sc, nullptr, nullptr,
	                                        &Swapchain),
	      "Create window swapchain");
	Check(Swapchain.As(&P.Swapchain), "Query swapchain");
	Check(P.Factory->MakeWindowAssociation(static_cast<HWND>(InSurface.Handle), DXGI_MWA_NO_ALT_ENTER),
	      "Window association");
	P.Buffers();
	for (auto& Frame : P.Frames)
	{
		for (auto& Allocator : Frame.Allocators)
		{
			Check(P.State->Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Allocator)),
			      "Create frame allocator");
		}
	}
	Log(ELogLevel::Info, "D3D12 adapter: " + P.AdapterName + "; debug layer: " + (P.Debug ? "enabled" : "unavailable"));
}

FRhiDevice::~FRhiDevice()
{
	if (Impl && Impl->Queue)
	{
		try
		{
			Impl->Idle();
		}
		catch (const std::exception& E)
		{
			Log(ELogLevel::Error, E.what());
		}
	}
}

FBuffer FRhiDevice::CreateBuffer(std::span<const std::byte> InBytes)
{
	if (InBytes.empty())
	{
		throw std::invalid_argument("Empty GPU buffer");
	}
	auto R = Impl->AllocateBuffer(InBytes.size(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	void* Mapped{};
	D3D12_RANGE Read{0, 0};
	Check(R.Impl->Resource->Map(0, &Read, &Mapped), "Map upload buffer");
	std::memcpy(Mapped, InBytes.data(), InBytes.size());
	R.Impl->Resource->Unmap(0, nullptr);
	return R;
}

FTexture FRhiDevice::CreateTexture(const FImage& InImage)
{
	auto& P = *Impl;
	if (!InImage.Width || !InImage.Height || InImage.Width > 16384 || InImage.Height > 16384 ||
	    InImage.Rgba.size() != std::size_t(InImage.Width) * InImage.Height * 4)
	{
		throw std::invalid_argument("Invalid texture image");
	}
	auto R = std::make_shared<FTextureImpl>();
	R->State = P.State;
	D3D12_RESOURCE_DESC D{};
	D.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	D.Width = InImage.Width;
	D.Height = InImage.Height;
	D.DepthOrArraySize = 1;
	D.MipLevels = 1;
	D.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	D.SampleDesc.Count = 1;
	D3D12MA::ALLOCATION_DESC A{};
	A.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	Check(P.State->Allocator->CreateResource(&A, &D, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, &R->Allocation,
	                                         IID_PPV_ARGS(&R->Resource)),
	      "Allocate texture");
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint{};
	UINT64 Total{};
	P.State->Device->GetCopyableFootprints(&D, 0, 1, 0, &Footprint, nullptr, nullptr, &Total);
	auto Upload = P.AllocateBuffer(Total, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	void* Mapped{};
	D3D12_RANGE Read{0, 0};
	Check(Upload.Impl->Resource->Map(0, &Read, &Mapped), "Map texture upload");
	for (UINT Y = 0; Y < InImage.Height; ++Y)
	{
		auto Row = static_cast<unsigned char*>(Mapped) + std::size_t(Y) * Footprint.Footprint.RowPitch;
		for (UINT X = 0; X < InImage.Width * 4; ++X)
		{
			Row[X] = static_cast<unsigned char>(
			    std::lround(std::clamp(InImage.Rgba[std::size_t(Y) * InImage.Width * 4 + X], 0.f, 1.f) * 255.f));
		}
	}
	Upload.Impl->Resource->Unmap(0, nullptr);
	P.Immediate(
	    [&](ID3D12GraphicsCommandList* InList)
	    {
		    D3D12_TEXTURE_COPY_LOCATION Dst{};
		    Dst.pResource = R->Resource.Get();
		    Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		    D3D12_TEXTURE_COPY_LOCATION Src{};
		    Src.pResource = Upload.Impl->Resource.Get();
		    Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		    Src.PlacedFootprint = Footprint;
		    InList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
		    Transition(InList, R->Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	    });
	R->Slot = P.State->Reserve();
	D3D12_SHADER_RESOURCE_VIEW_DESC Srv{};
	Srv.Format = D.Format;
	Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	Srv.Texture2D.MipLevels = 1;
	P.State->Device->CreateShaderResourceView(R->Resource.Get(), &Srv, P.State->Cpu(R->Slot));
	return {std::move(R)};
}

FPipeline FRhiDevice::CreatePipeline(const FPipelineDesc& InDesc)
{
	if (InDesc.Vertex.Format != EShaderFormat::Dxil || InDesc.Pixel.Format != EShaderFormat::Dxil ||
	    InDesc.Vertex.Bytes.empty() || InDesc.Pixel.Bytes.empty())
	{
		throw std::invalid_argument("DX12 pipeline requires DXIL shaders");
	}
	auto R = std::make_shared<FPipelineImpl>();
	R->Textured = InDesc.Textured;
	D3D12_DESCRIPTOR_RANGE Range{};
	Range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	Range.NumDescriptors = 1;
	Range.BaseShaderRegister = 0;
	D3D12_ROOT_PARAMETER Parameters[2]{};
	Parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	Parameters[0].Constants = {0, 0, 16};
	Parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	Parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	Parameters[1].DescriptorTable = {1, &Range};
	Parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	D3D12_STATIC_SAMPLER_DESC Sampler{};
	Sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	Sampler.AddressU = Sampler.AddressV = Sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	Sampler.MaxLOD = D3D12_FLOAT32_MAX;
	Sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	Sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	D3D12_ROOT_SIGNATURE_DESC Root{};
	Root.NumParameters = InDesc.Textured ? 2 : 1;
	Root.pParameters = Parameters;
	Root.NumStaticSamplers = InDesc.Textured ? 1 : 0;
	Root.pStaticSamplers = &Sampler;
	Root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ComPtr<ID3DBlob> Blob;
	ComPtr<ID3DBlob> Error;
	Check(D3D12SerializeRootSignature(&Root, D3D_ROOT_SIGNATURE_VERSION_1, &Blob, &Error), "Serialize root signature");
	Check(Impl->State->Device->CreateRootSignature(0, Blob->GetBufferPointer(), Blob->GetBufferSize(),
	                                               IID_PPV_ARGS(&R->Root)),
	      "Create root signature");
	std::vector<D3D12_INPUT_ELEMENT_DESC> Attributes;
	for (const auto& A : InDesc.Attributes)
	{
		DXGI_FORMAT F = DXGI_FORMAT_R32G32_FLOAT;
		switch (A.Format)
		{
			case EVertexFormat::Float2:
				break;
			case EVertexFormat::Float3:
				F = DXGI_FORMAT_R32G32B32_FLOAT;
				break;
			case EVertexFormat::Float4:
				F = DXGI_FORMAT_R32G32B32A32_FLOAT;
				break;
			case EVertexFormat::Unorm8x4:
				F = DXGI_FORMAT_R8G8B8A8_UNORM;
				break;
		}
		Attributes.push_back(
		    {A.Semantic.c_str(), A.SemanticIndex, F, 0, A.Offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC Pso{};
	Pso.pRootSignature = R->Root.Get();
	Pso.VS = {InDesc.Vertex.Bytes.data(), InDesc.Vertex.Bytes.size()};
	Pso.PS = {InDesc.Pixel.Bytes.data(), InDesc.Pixel.Bytes.size()};
	Pso.InputLayout = {Attributes.data(), static_cast<UINT>(Attributes.size())};
	Pso.SampleMask = UINT_MAX;
	Pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	Pso.NumRenderTargets = 1;
	Pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	Pso.SampleDesc.Count = 1;
	Pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	Pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	Pso.RasterizerState.DepthClipEnable = TRUE;
	Pso.DepthStencilState.DepthEnable = FALSE;
	Pso.DepthStencilState.StencilEnable = FALSE;
	auto& Blend = Pso.BlendState.RenderTarget[0];
	Blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	Blend.BlendEnable = InDesc.AlphaBlend;
	Blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	Blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	Blend.BlendOp = D3D12_BLEND_OP_ADD;
	Blend.SrcBlendAlpha = D3D12_BLEND_ONE;
	Blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	Blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	Check(Impl->State->Device->CreateGraphicsPipelineState(&Pso, IID_PPV_ARGS(&R->Pipeline)),
	      "Create graphics pipeline");
	return {std::move(R)};
}

void FRhiDevice::BeginFrame(FSize InSize)
{
	auto& P = *Impl;
	if (P.Active)
	{
		throw std::logic_error("Frame already active");
	}
	if (!InSize.Width || !InSize.Height)
	{
		throw std::invalid_argument("Zero-sized frame");
	}
	if (InSize.Width != P.Size.Width || InSize.Height != P.Size.Height)
	{
		P.Idle();
		for (auto& B : P.Backbuffers)
		{
			B.Reset();
		}
		Check(P.Swapchain->ResizeBuffers(FrameCount, InSize.Width, InSize.Height, DXGI_FORMAT_R8G8B8A8_UNORM, 0),
		      "Resize swapchain");
		P.Size = InSize;
		P.Buffers();
	}
	P.FrameIndex = P.Swapchain->GetCurrentBackBufferIndex();
	auto& F = P.Frames[P.FrameIndex];
	P.Wait(F.FenceValue);
	F.Retained.clear();
	for (auto& B : F.Recorded)
	{
		B = false;
	}
	P.Active = true;
	++P.Serial;
}

FRecordedList FRhiDevice::Record(std::uint32_t InContext, const FPassCommands& InCommands)
{
	auto& P = *Impl;
	if (!P.Active || InContext >= ContextCount)
	{
		throw std::invalid_argument("Invalid recording context");
	}
	auto& Frame = P.Frames[P.FrameIndex];
	if (Frame.Recorded[InContext].exchange(true))
	{
		throw std::logic_error("Command context already recorded this frame");
	}
	FProfileScope Trace(InCommands.Name.c_str());
	Check(Frame.Allocators[InContext]->Reset(), "Reset command allocator");
	auto R = std::make_shared<FRecordedListImpl>();
	R->Frame = P.Serial;
	R->Context = InContext;
	R->Owner = &P;
	R->Retained = InCommands.Draws;
	Check(P.State->Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Frame.Allocators[InContext].Get(),
	                                         nullptr, IID_PPV_ARGS(&R->List)),
	      "Create recording list");
	auto List = R->List.Get();
	if (InCommands.TransitionFrom && InCommands.TransitionTo)
	{
		Transition(List, P.Backbuffers[P.FrameIndex].Get(), Native(*InCommands.TransitionFrom),
		           Native(*InCommands.TransitionTo));
	}
	auto Rtv = P.Rtvs->GetCPUDescriptorHandleForHeapStart();
	Rtv.ptr += std::size_t(P.FrameIndex) * P.RtvStep;
	List->OMSetRenderTargets(1, &Rtv, FALSE, nullptr);
	if (InCommands.Clear)
	{
		float Color[] = {InCommands.ClearColor.X, InCommands.ClearColor.Y, InCommands.ClearColor.Z,
		                 InCommands.ClearColor.W};
		List->ClearRenderTargetView(Rtv, Color, 0, nullptr);
	}
	D3D12_VIEWPORT Viewport{0, 0, static_cast<float>(P.Size.Width), static_cast<float>(P.Size.Height), 0, 1};
	List->RSSetViewports(1, &Viewport);
	List->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12DescriptorHeap* Heaps[] = {P.State->Textures.Get()};
	List->SetDescriptorHeaps(1, Heaps);
	for (const auto& Draw : InCommands.Draws)
	{
		if (!Draw.Pipeline || !Draw.Vertices || !Draw.Indices || !Draw.VertexStride ||
		    std::uint64_t(Draw.FirstIndex + std::uint64_t(Draw.IndexCount)) * 4 > Draw.Indices.Impl->Size)
		{
			throw std::invalid_argument("Invalid draw packet");
		}
		List->SetPipelineState(Draw.Pipeline.Impl->Pipeline.Get());
		List->SetGraphicsRootSignature(Draw.Pipeline.Impl->Root.Get());
		List->SetGraphicsRoot32BitConstants(0, 16, Draw.Constants.Values.data(), 0);
		if (Draw.Pipeline.Impl->Textured)
		{
			if (!Draw.Texture)
			{
				throw std::invalid_argument("Textured draw has no texture");
			}
			List->SetGraphicsRootDescriptorTable(1, P.State->Gpu(Draw.Texture.Impl->Slot));
		}
		D3D12_VERTEX_BUFFER_VIEW Vb{Draw.Vertices.Impl->Resource->GetGPUVirtualAddress(),
		                            static_cast<UINT>(Draw.Vertices.Impl->Size), Draw.VertexStride};
		D3D12_INDEX_BUFFER_VIEW Ib{Draw.Indices.Impl->Resource->GetGPUVirtualAddress(),
		                           static_cast<UINT>(Draw.Indices.Impl->Size), DXGI_FORMAT_R32_UINT};
		List->IASetVertexBuffers(0, 1, &Vb);
		List->IASetIndexBuffer(&Ib);
		D3D12_RECT Rect{Draw.Scissor.Left, Draw.Scissor.Top, Draw.Scissor.Right, Draw.Scissor.Bottom};
		List->RSSetScissorRects(1, &Rect);
		List->DrawIndexedInstanced(Draw.IndexCount, 1, Draw.FirstIndex, Draw.VertexOffset, 0);
	}
	Check(List->Close(), "Close recording list");
	return {std::move(R)};
}

FImage FRhiDevice::EndFrame(std::span<const FRecordedList> InLists, bool InVsync, bool InCapture)
{
	auto& P = *Impl;
	if (!P.Active || InLists.empty())
	{
		throw std::logic_error("No active frame commands");
	}
	auto& Frame = P.Frames[P.FrameIndex];
	std::vector<ID3D12CommandList*> NativeLists;
	std::array<bool, ContextCount> Seen{};
	for (const auto& List : InLists)
	{
		if (!List.Impl || List.Impl->Frame != P.Serial || List.Impl->Owner != &P || Seen[List.Impl->Context])
		{
			throw std::invalid_argument("Stale, foreign or duplicate command list");
		}
		Seen[List.Impl->Context] = true;
		NativeLists.push_back(List.Impl->List.Get());
	}
	Frame.Retained.assign(InLists.begin(), InLists.end());
	P.Queue->ExecuteCommandLists(static_cast<UINT>(NativeLists.size()), NativeLists.data());
	FImage Result;
	if (InCapture)
	{
		auto Desc = P.Backbuffers[P.FrameIndex]->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint{};
		UINT64 Total{};
		P.State->Device->GetCopyableFootprints(&Desc, 0, 1, 0, &Footprint, nullptr, nullptr, &Total);
		auto Readback = P.AllocateBuffer(Total, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
		P.Immediate(
		    [&](ID3D12GraphicsCommandList* InList)
		    {
			    auto Resource = P.Backbuffers[P.FrameIndex].Get();
			    Transition(InList, Resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
			    D3D12_TEXTURE_COPY_LOCATION Src{};
			    Src.pResource = Resource;
			    Src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			    D3D12_TEXTURE_COPY_LOCATION Dst{};
			    Dst.pResource = Readback.Impl->Resource.Get();
			    Dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			    Dst.PlacedFootprint = Footprint;
			    InList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
			    Transition(InList, Resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
		    });
		Result = {P.Size.Width, P.Size.Height, EColorSpace::Srgb, {}};
		Result.Rgba.resize(std::size_t(P.Size.Width) * P.Size.Height * 4);
		void* Mapped{};
		D3D12_RANGE Range{0, static_cast<SIZE_T>(Total)};
		Check(Readback.Impl->Resource->Map(0, &Range, &Mapped), "Map screenshot");
		for (UINT Y = 0; Y < P.Size.Height; ++Y)
		{
			auto Row = static_cast<unsigned char*>(Mapped) + std::size_t(Y) * Footprint.Footprint.RowPitch;
			for (UINT X = 0; X < P.Size.Width * 4; ++X)
			{
				Result.Rgba[std::size_t(Y) * P.Size.Width * 4 + X] = Row[X] / 255.f;
			}
		}
		D3D12_RANGE Written{0, 0};
		Readback.Impl->Resource->Unmap(0, &Written);
	}
	auto Hr = P.Swapchain->Present(InVsync ? 1 : 0, 0);
	Frame.FenceValue = P.Signal();
	P.Active = false;
	++P.Submitted;
	Check(Hr, "Present swapchain");
	return Result;
}

void FRhiDevice::WaitIdle()
{
	Impl->Idle();
}

FDeviceStats FRhiDevice::Statistics() const
{
	auto& P = *Impl;
	FDeviceStats Stats{P.AdapterName, P.Debug, 0, P.Submitted, 0};
	D3D12MA::Budget Local{};
	D3D12MA::Budget Nonlocal{};
	P.State->Allocator->GetBudget(&Local, &Nonlocal);
	Stats.GpuAllocationBytes = Local.Stats.AllocationBytes + Nonlocal.Stats.AllocationBytes;
	if (P.Info)
	{
		for (UINT64 I = 0; I < P.Info->GetNumStoredMessages(); ++I)
		{
			SIZE_T Size{};
			P.Info->GetMessage(I, nullptr, &Size);
			std::vector<std::byte> Bytes(Size);
			auto Msg = reinterpret_cast<D3D12_MESSAGE*>(Bytes.data());
			if (SUCCEEDED(P.Info->GetMessage(I, Msg, &Size)) &&
			    (Msg->Severity == D3D12_MESSAGE_SEVERITY_ERROR || Msg->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION))
			{
				++Stats.ValidationErrors;
				Log(ELogLevel::Error, Msg->pDescription);
			}
		}
	}
	return Stats;
}
} // namespace Hyperion
