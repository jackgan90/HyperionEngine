#include "D3D12RHIDevice.h"
#include "D3D12RHISwapchain.h"
#include "D3D12Resources.h"
#include "Hyperion/Core/Core.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Hyperion
{
FD3D12RHIDevice::FD3D12RHIDevice(const FRHIDeviceDesc& InDesc) : State(std::make_shared<FD3D12DeviceState>())
{
	auto& P = *State;
	static std::once_flag DebugOnce;
	static bool DebugEnabled = false;
	std::call_once(DebugOnce,
	               [&]()
	               {
		               if (InDesc.EnableDebug)
		               {
			               ComPtr<ID3D12Debug> Layer;
			               if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&Layer))))
			               {
				               Layer->EnableDebugLayer();
				               DebugEnabled = true;
			               }
		               }
	               });
	P.Debug = DebugEnabled;
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
		if (SUCCEEDED(D3D12CreateDevice(Candidate.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&P.Device))))
		{
			P.Adapter = Candidate;
			P.AdapterName = Utf8(Desc.Description);
			break;
		}
	}
	if (!P.Device)
	{
		throw std::runtime_error("No hardware D3D12 feature-level 12.0 adapter found");
	}
	if (P.Debug)
	{
		P.Device.As(&P.Info);
	}

	auto& Caps = P.Capabilities;
	Caps.Backend = ERHIBackend::D3D12;
	Caps.ShaderFormat = EShaderFormat::Dxil;
	Caps.Adapter = P.AdapterName;
	Caps.MaxRecordingContexts = ContextCount;
	Caps.MaxSampledTextures = TextureCount;
	Caps.MaxTextureDimension = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	for (const auto Feature :
	     {ERHIFeature::Graphics, ERHIFeature::TextureSampling, ERHIFeature::ConcurrentRecording, ERHIFeature::Readback})
	{
		Caps.Features[static_cast<std::size_t>(Feature)] = {true, true};
	}
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 Options5{};
	if (SUCCEEDED(P.Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &Options5, sizeof(Options5))))
	{
		Caps.Features[static_cast<std::size_t>(ERHIFeature::RayTracing)].Supported =
		    Options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	}
	D3D12_FEATURE_DATA_D3D12_OPTIONS7 Options7{};
	if (SUCCEEDED(P.Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &Options7, sizeof(Options7))))
	{
		Caps.Features[static_cast<std::size_t>(ERHIFeature::MeshShaders)].Supported =
		    Options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
	}
	ValidateRequiredFeatures(InDesc, Caps);
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
	Ma.pDevice = P.Device.Get();
	Ma.pAdapter = P.Adapter.Get();
	Ma.pAllocationCallbacks = &Callbacks;
	Check(D3D12MA::CreateAllocator(&Ma, &P.Allocator), "Create GPU allocator");
	D3D12_DESCRIPTOR_HEAP_DESC Heap{};
	Heap.NumDescriptors = TextureCount;
	Heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	Heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	Check(P.Device->CreateDescriptorHeap(&Heap, IID_PPV_ARGS(&P.Textures)), "Texture descriptor heap");
	P.TextureStep = P.Device->GetDescriptorHandleIncrementSize(Heap.Type);
	D3D12_COMMAND_QUEUE_DESC Q{};
	Q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	Check(P.Device->CreateCommandQueue(&Q, IID_PPV_ARGS(&P.Queue)), "Create graphics queue");
	Check(P.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&P.Fence)), "Create GPU fence");
	P.Event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!P.Event)
	{
		throw std::runtime_error("Create fence event failed");
	}

	Log(ELogLevel::Info, "D3D12 adapter: " + P.AdapterName + "; debug layer: " + (P.Debug ? "enabled" : "unavailable"));
}

FD3D12RHIDevice::~FD3D12RHIDevice()
{
	try
	{
		State->Idle();
	}
	catch (const std::exception& Error)
	{
		Log(ELogLevel::Error, Error.what());
	}
}

const FRHICapabilities& FD3D12RHIDevice::GetCapabilities() const noexcept
{
	return State->Capabilities;
}

FRHIFeatureSupport FD3D12RHIDevice::QueryFeature(ERHIFeature InFeature) const
{
	return State->Capabilities.QueryFeature(InFeature);
}

std::unique_ptr<IRHISwapchain> FD3D12RHIDevice::CreateSwapchain(const FRHISwapchainDesc& InDesc)
{
	return std::make_unique<FD3D12RHISwapchain>(State, InDesc);
}

void FD3D12RHIDevice::WaitIdle()
{
	State->Idle();
}

FBuffer FD3D12RHIDevice::CreateBuffer(std::span<const std::byte> InBytes)
{
	if (InBytes.empty())
	{
		throw std::invalid_argument("Empty GPU buffer");
	}
	auto R = State->AllocateBuffer(InBytes.size(), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	void* Mapped{};
	D3D12_RANGE Read{0, 0};
	Check(R->Resource->Map(0, &Read, &Mapped), "Map upload buffer");
	std::memcpy(Mapped, InBytes.data(), InBytes.size());
	R->Resource->Unmap(0, nullptr);
	return {std::move(R)};
}

FTexture FD3D12RHIDevice::CreateTexture(const FImage& InImage)
{
	auto& P = *State;
	if (!InImage.Width || !InImage.Height || InImage.Width > 16384 || InImage.Height > 16384 ||
	    InImage.Rgba.size() != std::size_t(InImage.Width) * InImage.Height * 4)
	{
		throw std::invalid_argument("Invalid texture image");
	}
	auto R = std::make_shared<FD3D12Texture>();
	R->State = State;
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
	Check(P.Allocator->CreateResource(&A, &D, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, &R->Allocation,
	                                  IID_PPV_ARGS(&R->Resource)),
	      "Allocate texture");
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint{};
	UINT64 Total{};
	P.Device->GetCopyableFootprints(&D, 0, 1, 0, &Footprint, nullptr, nullptr, &Total);
	auto Upload = P.AllocateBuffer(Total, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
	void* Mapped{};
	D3D12_RANGE Read{0, 0};
	Check(Upload->Resource->Map(0, &Read, &Mapped), "Map texture upload");
	for (UINT Y = 0; Y < InImage.Height; ++Y)
	{
		auto Row = static_cast<unsigned char*>(Mapped) + std::size_t(Y) * Footprint.Footprint.RowPitch;
		for (UINT X = 0; X < InImage.Width * 4; ++X)
		{
			Row[X] = static_cast<unsigned char>(
			    std::lround(std::clamp(InImage.Rgba[std::size_t(Y) * InImage.Width * 4 + X], 0.f, 1.f) * 255.f));
		}
	}
	Upload->Resource->Unmap(0, nullptr);
	P.Immediate(
	    [&](ID3D12GraphicsCommandList* InList)
	    {
		    D3D12_TEXTURE_COPY_LOCATION Dst{};
		    Dst.pResource = R->Resource.Get();
		    Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		    D3D12_TEXTURE_COPY_LOCATION Src{};
		    Src.pResource = Upload->Resource.Get();
		    Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		    Src.PlacedFootprint = Footprint;
		    InList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
		    Transition(InList, R->Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
		               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	    });
	R->Slot = P.Reserve();
	D3D12_SHADER_RESOURCE_VIEW_DESC Srv{};
	Srv.Format = D.Format;
	Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	Srv.Texture2D.MipLevels = 1;
	P.Device->CreateShaderResourceView(R->Resource.Get(), &Srv, P.Cpu(R->Slot));
	return {std::move(R)};
}

FPipeline FD3D12RHIDevice::CreatePipeline(const FPipelineDesc& InDesc)
{
	if (InDesc.Vertex.Format != EShaderFormat::Dxil || InDesc.Pixel.Format != EShaderFormat::Dxil ||
	    InDesc.Vertex.Bytes.empty() || InDesc.Pixel.Bytes.empty())
	{
		throw std::invalid_argument("DX12 pipeline requires DXIL shaders");
	}
	auto R = std::make_shared<FD3D12Pipeline>();
	R->State = State;
	R->Textured = InDesc.Textured;
	R->MaterialLayout = InDesc.MaterialLayout;
	D3D12_DESCRIPTOR_RANGE Range{};
	Range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	Range.NumDescriptors = 1;
	Range.BaseShaderRegister = 0;
	D3D12_ROOT_PARAMETER Parameters[6]{};
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
	std::array<D3D12_DESCRIPTOR_RANGE, 5> MaterialRanges{};
	std::array<D3D12_STATIC_SAMPLER_DESC, 5> MaterialSamplers{};
	if (InDesc.MaterialLayout)
	{
		Parameters[0] = {};
		Parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		Parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		const auto Address = [](ERHIAddressMode InMode)
		{
			return InMode == ERHIAddressMode::Clamp    ? D3D12_TEXTURE_ADDRESS_MODE_CLAMP
			       : InMode == ERHIAddressMode::Mirror ? D3D12_TEXTURE_ADDRESS_MODE_MIRROR
			                                           : D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		};
		for (UINT Index = 0; Index < 5; ++Index)
		{
			MaterialRanges[Index] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, Index, 0, 0};
			Parameters[Index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			Parameters[Index + 1].DescriptorTable = {1, &MaterialRanges[Index]};
			Parameters[Index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			const auto& Source = InDesc.Samplers[Index];
			auto& NativeSampler = MaterialSamplers[Index];
			NativeSampler.Filter = static_cast<D3D12_FILTER>((Source.MinLinear ? 16 : 0) | (Source.MagLinear ? 4 : 0) |
			                                                 (Source.MipLinear ? 1 : 0));
			NativeSampler.AddressU = Address(Source.U);
			NativeSampler.AddressV = Address(Source.V);
			NativeSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			NativeSampler.MaxLOD = Source.Mipmapped ? D3D12_FLOAT32_MAX : 0;
			NativeSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			NativeSampler.ShaderRegister = Index;
			NativeSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
		Root.NumParameters = 6;
		Root.NumStaticSamplers = 5;
		Root.pStaticSamplers = MaterialSamplers.data();
	}
	ComPtr<ID3DBlob> Blob;
	ComPtr<ID3DBlob> Error;
	Check(D3D12SerializeRootSignature(&Root, D3D_ROOT_SIGNATURE_VERSION_1, &Blob, &Error), "Serialize root signature");
	Check(
	    State->Device->CreateRootSignature(0, Blob->GetBufferPointer(), Blob->GetBufferSize(), IID_PPV_ARGS(&R->Root)),
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
	Pso.RTVFormats[0] = InDesc.SrgbTarget ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
	Pso.SampleDesc.Count = 1;
	Pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	Pso.RasterizerState.CullMode = InDesc.CullBack ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
	Pso.RasterizerState.FrontCounterClockwise = InDesc.FrontCounterClockwise;
	Pso.RasterizerState.DepthClipEnable = TRUE;
	Pso.DepthStencilState.DepthEnable = InDesc.DepthTest;
	Pso.DepthStencilState.DepthWriteMask = InDesc.DepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	Pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	Pso.DSVFormat = InDesc.DepthTest ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
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
	Check(State->Device->CreateGraphicsPipelineState(&Pso, IID_PPV_ARGS(&R->Pipeline)), "Create graphics pipeline");
	return {std::move(R)};
}

FDeviceStats FD3D12RHIDevice::Statistics() const
{
	auto& P = *State;
	FDeviceStats Stats{P.AdapterName, P.Debug, 0, P.Submitted, 0};
	D3D12MA::Budget Local{};
	D3D12MA::Budget Nonlocal{};
	P.Allocator->GetBudget(&Local, &Nonlocal);
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
