#include "D3D12RHIDevice.h"
#include "D3D12GraphicsState.h"
#include "D3D12RHISwapchain.h"
#include "D3D12Resources.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Hyperion
{
namespace
{
void InitializeCapabilities(FD3D12DeviceState& InState, const FRHIDeviceDesc& InDesc)
{
	auto& Caps = InState.Capabilities;
	Caps.Backend = ERHIBackend::D3D12;
	Caps.ShaderFormat = EShaderFormat::Dxil;
	Caps.Adapter = InState.AdapterName;
	Caps.MaxRecordingContexts = ContextCount;
	Caps.MaxSampledTextures = 128;
	Caps.MaxRegisterSpaces = ShaderRegisterSpaceCount;
	Caps.MaxConstantBuffers = 14;
	Caps.MaxSamplers = 16;
	Caps.MaxReadBuffers = 128;
	Caps.ConstantAlignment = 256;
	Caps.MaxConstantRange = 65536;
	Caps.MaxAnisotropy = 16;
	Caps.bReadOnlyBuffers = true;
	Caps.bVertexTextures = true;
	if (InDesc.ResourceDescriptorCapacity == 0 || InDesc.ResourceDescriptorCapacity > 1000000 ||
	    InDesc.SamplerDescriptorCapacity == 0 || InDesc.SamplerDescriptorCapacity > 2048)
	{
		throw std::invalid_argument("Invalid material descriptor heap capacities");
	}
	Caps.ResourceDescriptorCapacity = InDesc.ResourceDescriptorCapacity;
	Caps.SamplerDescriptorCapacity = InDesc.SamplerDescriptorCapacity;
	Caps.MaxTextureDimension = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	for (const auto Feature : {ERHIFeature::Graphics, ERHIFeature::TextureSampling, ERHIFeature::ConcurrentRecording,
	                           ERHIFeature::Readback, ERHIFeature::InstancedDrawing})
	{
		Caps.Features[static_cast<std::size_t>(Feature)] = {true, true};
	}
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 Options5{};
	if (SUCCEEDED(InState.Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &Options5, sizeof(Options5))))
	{
		Caps.Features[static_cast<std::size_t>(ERHIFeature::RayTracing)].bSupported =
		    Options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	}
	D3D12_FEATURE_DATA_D3D12_OPTIONS7 Options7{};
	if (SUCCEEDED(InState.Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &Options7, sizeof(Options7))))
	{
		Caps.Features[static_cast<std::size_t>(ERHIFeature::MeshShaders)].bSupported =
		    Options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
	}
	ValidateRequiredFeatures(InDesc, Caps);
}

} // namespace

FD3D12RHIDevice::FD3D12RHIDevice(const FRHIDeviceDesc& InDesc) : State(std::make_shared<FD3D12DeviceState>())
{
	auto& P = *State;
	static std::once_flag DebugOnce;
	static bool bDebugEnabled = false;
	std::call_once(DebugOnce,
	               [&]()
	               {
		               if (InDesc.bEnableDebug)
		               {
			               ComPtr<ID3D12Debug> Layer;
			               if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&Layer))))
			               {
				               Layer->EnableDebugLayer();
				               bDebugEnabled = true;
			               }
		               }
	               });
	P.bDebug = bDebugEnabled;
	Check(CreateDXGIFactory2(P.bDebug ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&P.Factory)),
	      "Create DXGI factory");
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
	if (P.bDebug)
	{
		P.Device.As(&P.Info);
	}

	InitializeCapabilities(P, InDesc);
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
	P.ResourceSources.Initialize(*P.Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	                             P.Capabilities.ResourceDescriptorCapacity, false);
	P.ResourceTables.Initialize(*P.Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	                            P.Capabilities.ResourceDescriptorCapacity, true);
	P.SamplerSources.Initialize(*P.Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
	                            P.Capabilities.SamplerDescriptorCapacity, false);
	P.SamplerTables.Initialize(*P.Device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
	                           P.Capabilities.SamplerDescriptorCapacity, true);
	D3D12_COMMAND_QUEUE_DESC Q{};
	Q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	Check(P.Device->CreateCommandQueue(&Q, IID_PPV_ARGS(&P.Queue)), "Create graphics queue");
	Check(P.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&P.Fence)), "Create GPU fence");
	P.Event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!P.Event)
	{
		throw std::runtime_error("Create fence event failed");
	}

	Log(ELogLevel::Info,
	    "D3D12 adapter: " + P.AdapterName + "; debug layer: " + (P.bDebug ? "enabled" : "unavailable"));
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
	return CreateBuffer({InBytes.size(), BufferUsage(ERHIBufferUsage::Vertex) | BufferUsage(ERHIBufferUsage::Index)},
	                    InBytes);
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
		               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	    },
	    {R->Resource, Upload->Resource}, {R->Allocation, Upload->Allocation});
	D3D12_SHADER_RESOURCE_VIEW_DESC Srv{};
	Srv.Format = D.Format;
	Srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	Srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	Srv.Texture2D.MipLevels = 1;
	R->SourceDescriptor = P.ResourceSources.Reserve(1);
	P.Device->CreateShaderResourceView(R->Resource.Get(), &Srv, P.ResourceSources.Cpu(R->SourceDescriptor.Offset));
	++P.DescriptorAllocations;
	return {std::move(R)};
}

FPipeline FD3D12RHIDevice::CreatePipeline(const FPipelineDesc& InDesc)
{
	if (InDesc.Vertex.Format != EShaderFormat::Dxil || InDesc.Vertex.Bytes.empty() ||
	    (!InDesc.Pixel.Bytes.empty() && InDesc.Pixel.Format != EShaderFormat::Dxil))
	{
		throw std::invalid_argument("DX12 pipeline requires DXIL shaders");
	}
	auto R = std::make_shared<FD3D12Pipeline>();
	R->State = State;
	R->Layout = InDesc.Layout;
	ValidateGraphicsPipeline(InDesc);
	const auto& Layout = NativeResource<FD3D12BindingLayout>(R->Layout.Payload, State.get());
	ValidatePipelineBindings(InDesc, Layout.Description);
	R->Root = Layout.Root;
	R->Target = InDesc.Target;
	R->GraphicsState = InDesc.State;
	R->VertexStride = InDesc.VertexStride;
	R->Topology = InDesc.Topology;
	std::vector<D3D12_INPUT_ELEMENT_DESC> Attributes;
	for (const auto& A : InDesc.Attributes)
	{
		Attributes.push_back({A.Semantic.c_str(), A.SemanticIndex, NativeVertexFormat(A.Format), 0, A.Offset,
		                      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0});
	}
	D3D12_GRAPHICS_PIPELINE_STATE_DESC Pso{};
	Pso.pRootSignature = R->Root.Get();
	Pso.VS = {InDesc.Vertex.Bytes.data(), InDesc.Vertex.Bytes.size()};
	if (!InDesc.Pixel.Bytes.empty())
	{
		Pso.PS = {InDesc.Pixel.Bytes.data(), InDesc.Pixel.Bytes.size()};
	}
	Pso.InputLayout = {Attributes.data(), static_cast<UINT>(Attributes.size())};
	ApplyGraphicsState(Pso, InDesc);
	Check(State->Device->CreateGraphicsPipelineState(&Pso, IID_PPV_ARGS(&R->Pipeline)), "Create graphics pipeline");
	++State->PipelinesCreated;
	return {std::move(R)};
}

FDeviceStats FD3D12RHIDevice::Statistics() const
{
	auto& P = *State;
	FDeviceStats Stats{P.AdapterName, P.bDebug, 0, P.Submitted, 0};
	Stats.DescriptorAllocations = P.DescriptorAllocations;
	Stats.DescriptorCopies = P.DescriptorCopies;
	Stats.BindingSetsCreated = P.BindingSetsCreated;
	Stats.GraphicsRootBinds = P.GraphicsRootBinds.load();
	Stats.GraphicsHeapBinds = P.GraphicsHeapBinds.load();
	Stats.GraphicsConstantBinds = P.GraphicsConstantBinds.load();
	Stats.GraphicsTableBinds = P.GraphicsTableBinds.load();
	Stats.PipelinesCreated = P.PipelinesCreated;
	Stats.ConstantBytesWritten = P.ConstantBytesWritten;
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

void FD3D12RHIDevice::CollectCompletedResources()
{
	State->CollectUploads();
}
} // namespace Hyperion
