#include "D3D12RHISwapchain.h"
#include "D3D12Draws.h"
#include "D3D12GraphicsState.h"
#include "D3D12PassTimings.h"
#include "D3D12Resources.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Core/Profiling.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include <atomic>
#include <cmath>

namespace Hyperion
{
namespace
{
FSize ValidatePass(const FD3D12DeviceState& InState, const FPassCommands& InCommands, FSize InSize,
                   ERHIDepthFormat InDepthFormat)
{
	if ((!InCommands.bUseColor && InCommands.bClear) ||
	    (!InCommands.bUseColor && !InCommands.bUseDepth && !InCommands.GetDraws().empty()))
	{
		throw std::invalid_argument("Pass color clear/draw has no compatible attachment");
	}
	if (InCommands.DepthTarget)
	{
		const auto& Texture = NativeResource<FD3D12Texture>(InCommands.DepthTarget.Payload, &InState);
		if (!Texture.DepthViews || !InCommands.bUseDepth ||
		    (InCommands.bUseColor &&
		     (Texture.DepthSize.Width != InSize.Width || Texture.DepthSize.Height != InSize.Height)))
		{
			throw std::invalid_argument("Invalid depth target type or color/depth dimensions");
		}
		InSize = Texture.DepthSize;
		InDepthFormat = ERHIDepthFormat::D32;
	}
	for (const auto& Texture : InCommands.SampledDepth)
	{
		if (!NativeResource<FD3D12Texture>(Texture.Payload, &InState).DepthViews || Texture == InCommands.DepthTarget)
		{
			throw std::invalid_argument("Invalid sampled depth resource");
		}
	}
	for (const auto& Barrier : InCommands.TextureTransitions)
	{
		if (!NativeResource<FD3D12Texture>(Barrier.Texture.Payload, &InState).DepthViews ||
		    (Barrier.Before != EResourceState::ShaderRead && Barrier.Before != EResourceState::DepthWrite) ||
		    (Barrier.After != EResourceState::ShaderRead && Barrier.After != EResourceState::DepthWrite))
		{
			throw std::invalid_argument("Invalid depth resource transition");
		}
	}
	if ((InCommands.bClearDepth && !InCommands.bUseDepth) || (InCommands.bClearStencil && !InCommands.bUseStencil) ||
	    ((InCommands.bUseDepth || InCommands.bUseStencil) &&
	     (InCommands.DepthFormat != InDepthFormat || InDepthFormat == ERHIDepthFormat::None)) ||
	    (InCommands.bUseStencil && InDepthFormat != ERHIDepthFormat::D32S8) || !std::isfinite(InCommands.ClearDepth) ||
	    InCommands.ClearDepth < 0 || InCommands.ClearDepth > 1)
	{
		throw std::invalid_argument("Invalid depth/stencil pass attachment or clear");
	}
	if (InCommands.Viewport)
	{
		ValidateViewport(*InCommands.Viewport, InSize);
	}
	return InSize;
}

void RecordAttachments(ID3D12GraphicsCommandList& InList, const FPassCommands& InCommands, FSize InSize,
                       D3D12_CPU_DESCRIPTOR_HANDLE InRtv, D3D12_CPU_DESCRIPTOR_HANDLE InDsv)
{
	InList.OMSetRenderTargets(InCommands.bUseColor ? 1 : 0, InCommands.bUseColor ? &InRtv : nullptr, FALSE,
	                          (InCommands.bUseDepth || InCommands.bUseStencil) ? &InDsv : nullptr);
	const FViewport View = InCommands.Viewport.value_or(
	    FViewport{0, 0, static_cast<float>(InSize.Width), static_cast<float>(InSize.Height), 0, 1});
	D3D12_RECT ClearRect{static_cast<LONG>(std::floor(View.X)), static_cast<LONG>(std::floor(View.Y)),
	                     static_cast<LONG>(std::ceil(View.X + View.Width)),
	                     static_cast<LONG>(std::ceil(View.Y + View.Height))};
	if (InCommands.bClearDepth || InCommands.bClearStencil)
	{
		const auto Flags = static_cast<D3D12_CLEAR_FLAGS>((InCommands.bClearDepth ? D3D12_CLEAR_FLAG_DEPTH : 0) |
		                                                  (InCommands.bClearStencil ? D3D12_CLEAR_FLAG_STENCIL : 0));
		InList.ClearDepthStencilView(InDsv, Flags, InCommands.ClearDepth, InCommands.ClearStencil, 1, &ClearRect);
	}
	if (InCommands.bClear)
	{
		float Color[]{InCommands.ClearColor.X, InCommands.ClearColor.Y, InCommands.ClearColor.Z,
		              InCommands.ClearColor.W};
		InList.ClearRenderTargetView(InRtv, Color, 1, &ClearRect);
	}
	D3D12_VIEWPORT Viewport{View.X, View.Y, View.Width, View.Height, View.MinDepth, View.MaxDepth};
	InList.RSSetViewports(1, &Viewport);
}
} // namespace

#if defined(HYP_TEST_D3D12_PRESENT)
// Linked only by the native fault-injection test target; normal builds call DXGI directly.
HRESULT PresentForTesting(IDXGISwapChain3* InSwapchain, UINT InInterval);
#endif

struct FD3D12RHISwapchain::FImpl
{
	std::shared_ptr<FD3D12DeviceState> State;
	// Retained by lists so a new swapchain at a recycled address cannot accept old recordings.
	std::shared_ptr<const FD3D12SwapchainIdentity> Identity = std::make_shared<FD3D12SwapchainIdentity>();
	ComPtr<IDXGISwapChain3> Swapchain;
	ComPtr<ID3D12DescriptorHeap> Rtvs;
	ComPtr<ID3D12DescriptorHeap> Dsvs;
	ComPtr<ID3D12Resource> Depth;
	ComPtr<D3D12MA::Allocation> DepthAllocation;
	std::array<ComPtr<ID3D12Resource>, FrameCount> Backbuffers;

	struct FFrame
	{
		struct FRecordingStorage
		{
			ComPtr<ID3D12GraphicsCommandList> List;
			std::weak_ptr<const FD3D12RecordedList> Owner;
		};

		std::shared_ptr<FD3D12PassQueries> TimingQueries;
		bool bTimeFrame{};
#if HYP_ENABLE_PROFILING
		std::shared_ptr<FD3D12ProfileQueries> ProfileQueries;
#endif
		std::array<ComPtr<ID3D12CommandAllocator>, ContextCount> Allocators;
		std::array<FRecordingStorage, ContextCount> Recordings;
		std::array<std::atomic<bool>, ContextCount> Recorded{};
		std::vector<FRecordedList> Retained;
		std::uint64_t FenceValue{};
	};

	std::array<FFrame, FrameCount> Frames;
	std::array<FD3D12DrawCache, ContextCount> DrawCaches;
#if HYP_ENABLE_PROFILING
	FD3D12ProfileState Profiling;
#endif
	UINT FrameIndex{};
	UINT RtvStep{};
	std::uint64_t Serial{};
	FSize Size;
	ERHIDepthFormat DepthFormat = ERHIDepthFormat::D32;
	bool bActive{};
	bool bSubmissionStarted{};
	bool bGpuTiming{};

	std::shared_ptr<FD3D12RecordedList> PrepareRecording(std::uint32_t InContext,
	                                                     std::shared_ptr<const FPassCommands> InCommands)
	{
		HYP_PERF_SCOPE_C(Detail, PrepareRecordingStorage);
		auto Result = std::make_shared<FD3D12RecordedList>();
		Result->State = State;
		Result->Frame = Serial;
		Result->Context = InContext;
		Result->Owner = Identity;
		Result->Name = InCommands->Name;
		Result->Commands = std::move(InCommands);
		auto& Frame = Frames[FrameIndex];
		auto& Cached = Frame.Recordings[InContext];
		Check(Frame.Allocators[InContext]->Reset(), "Reset command allocator");
		if (Cached.List && Cached.Owner.expired())
		{
			HYP_PERF_SCOPE_C(Rhi, ResetNativeCommandList);
			// BeginFrame waited for this slot's fence. A retained logical recording prevents native list reuse.
			// Remove from the pool until Close succeeds so a failed recording cannot leave an open cached list.
			Result->List = std::move(Cached.List);
			Check(Result->List->Reset(Frame.Allocators[InContext].Get(), nullptr), "Reset recording list");
			++State->CommandListResets;
		}
		else
		{
			HYP_PERF_SCOPE_C(Rhi, CreateNativeCommandList);
			Check(State->Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Frame.Allocators[InContext].Get(),
			                                       nullptr, IID_PPV_ARGS(&Result->List)),
			      "Create recording list");
			++State->CommandListsCreated;
		}
		return Result;
	}

	void Idle()
	{
		State->Idle();
		for (auto& Frame : Frames)
		{
			CollectPassTimings(*State, Frame.Retained, 0);
#if HYP_ENABLE_PROFILING
			CollectD3D12Profiles(Frame.Retained);
#endif
			Frame.Retained.clear();
		}
	}

	void Buffers()
	{
		Depth.Reset();
		DepthAllocation.Reset();
		D3D12_RESOURCE_DESC Desc{};
		Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		Desc.Width = Size.Width;
		Desc.Height = Size.Height;
		Desc.DepthOrArraySize = 1;
		Desc.MipLevels = 1;
		Desc.Format = NativeDepthFormat(DepthFormat);
		Desc.SampleDesc.Count = 1;
		Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12MA::ALLOCATION_DESC Allocation{};
		Allocation.HeapType = D3D12_HEAP_TYPE_DEFAULT;
		D3D12_CLEAR_VALUE Clear{};
		Clear.Format = Desc.Format;
		Clear.DepthStencil.Depth = 1;
		Check(State->Allocator->CreateResource(&Allocation, &Desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &Clear,
		                                       &DepthAllocation, IID_PPV_ARGS(&Depth)),
		      "Create scene depth");
		State->Device->CreateDepthStencilView(Depth.Get(), nullptr, Dsvs->GetCPUDescriptorHandleForHeapStart());
		// Placed depth resources need whole-resource initialization before a partial viewport clear.
		// Graph compilation still requires an explicit clear before any logical depth/stencil load.
		State->Immediate(
		    [&](ID3D12GraphicsCommandList* InList)
		    {
			    InList->DiscardResource(Depth.Get(), nullptr);
		    },
		    {Depth}, {DepthAllocation});
		for (UINT I = 0; I < FrameCount; ++I)
		{
			Check(Swapchain->GetBuffer(I, IID_PPV_ARGS(&Backbuffers[I])), "Get swapchain buffer");
			auto H = Rtvs->GetCPUDescriptorHandleForHeapStart();
			H.ptr += std::size_t(I) * RtvStep;
			State->Device->CreateRenderTargetView(Backbuffers[I].Get(), nullptr, H);
			H.ptr += std::size_t(FrameCount) * RtvStep;
			D3D12_RENDER_TARGET_VIEW_DESC Srgb{};
			Srgb.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			Srgb.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			State->Device->CreateRenderTargetView(Backbuffers[I].Get(), &Srgb, H);
		}
	}
};

FD3D12RHISwapchain::FD3D12RHISwapchain(std::shared_ptr<FD3D12DeviceState> InState, const FRHISwapchainDesc& InDesc)
    : Impl(std::make_unique<FImpl>())
{
	if (!InDesc.Surface.Handle || !InDesc.Size.Width || !InDesc.Size.Height ||
	    (InDesc.DepthFormat != ERHIDepthFormat::D32 && InDesc.DepthFormat != ERHIDepthFormat::D32S8))
	{
		throw std::invalid_argument("Invalid RHI surface");
	}
	auto& P = *Impl;
	P.State = std::move(InState);
	P.Size = InDesc.Size;
	P.DepthFormat = InDesc.DepthFormat;
	D3D12_DESCRIPTOR_HEAP_DESC Heap{};
	Heap.NumDescriptors = FrameCount * 2;
	Heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	Check(P.State->Device->CreateDescriptorHeap(&Heap, IID_PPV_ARGS(&P.Rtvs)), "RTV heap");
	P.RtvStep = P.State->Device->GetDescriptorHandleIncrementSize(Heap.Type);
	Heap.NumDescriptors = 1;
	Heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	Check(P.State->Device->CreateDescriptorHeap(&Heap, IID_PPV_ARGS(&P.Dsvs)), "DSV heap");
	DXGI_SWAP_CHAIN_DESC1 Sc{};
	Sc.Width = InDesc.Size.Width;
	Sc.Height = InDesc.Size.Height;
	Sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Sc.SampleDesc.Count = 1;
	Sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Sc.BufferCount = FrameCount;
	Sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	ComPtr<IDXGISwapChain1> Swapchain;
	Check(P.State->Factory->CreateSwapChainForHwnd(P.State->Queue.Get(), static_cast<HWND>(InDesc.Surface.Handle), &Sc,
	                                               nullptr, nullptr, &Swapchain),
	      "Create window swapchain");
	Check(Swapchain.As(&P.Swapchain), "Query swapchain");
	Check(P.State->Factory->MakeWindowAssociation(static_cast<HWND>(InDesc.Surface.Handle), DXGI_MWA_NO_ALT_ENTER),
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
}

FD3D12RHISwapchain::~FD3D12RHISwapchain()
{
	try
	{
		Impl->Idle();
	}
	catch (const std::exception& Error)
	{
		Log(ELogLevel::Error, Error.what());
	}
}

const FRHICapabilities& FD3D12RHISwapchain::GetCapabilities() const noexcept
{
	return Impl->State->Capabilities;
}

void FD3D12RHISwapchain::BeginFrame(FSize InSize)
{
	HYP_PERF_SCOPE_C(Rhi, BeginFrame);
	auto& P = *Impl;
	if (P.bActive)
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
	P.State->Wait(F.FenceValue);
	// Direct RHI clients also retire completed lists during normal frame-ring progress.
	P.State->CollectUploads();
	F.Retained.clear();
	if (P.bGpuTiming && !F.TimingQueries)
	{
		F.TimingQueries = CreatePassQueries(*P.State);
	}
	F.bTimeFrame = P.bGpuTiming && F.TimingQueries && F.TimingQueries.use_count() == 1;
#if HYP_ENABLE_PROFILING
	PrepareD3D12Profiling(*P.State, P.Profiling, F.ProfileQueries);
#endif
	for (auto& bRecorded : F.Recorded)
	{
		bRecorded = false;
	}
	P.bActive = true;
	P.bSubmissionStarted = false;
	++P.Serial;
}

FRecordedList FD3D12RHISwapchain::Record(std::uint32_t InContext, const FPassCommands& InCommands)
{
	auto Copy = InCommands;
	Copy.MaterializeDraws();
	return RecordOwned(InContext, std::make_shared<const FPassCommands>(std::move(Copy)));
}

FRecordedList FD3D12RHISwapchain::RecordOwned(std::uint32_t InContext,
                                              std::shared_ptr<const FPassCommands> InOwnedCommands)
{
	HYP_PERF_SCOPE_C(Rhi, RecordPass);
	auto& P = *Impl;
	if (!P.bActive || InContext >= ContextCount || !InOwnedCommands)
	{
		throw std::invalid_argument("Invalid recording context");
	}
	const auto& InCommands = *InOwnedCommands;
	const auto TargetSize = ValidatePass(*P.State, InCommands, P.Size, P.DepthFormat);
	const auto DrawPlan = PrepareNativeDraws(*P.State, InOwnedCommands, P.DrawCaches[InContext]);
	auto& Frame = P.Frames[P.FrameIndex];
	if (Frame.Recorded[InContext].exchange(true))
	{
		throw std::logic_error("Command context already recorded this frame");
	}
	HYP_PERF_SCOPE_C(Rhi, RecordCommands);
	auto R = P.PrepareRecording(InContext, std::move(InOwnedCommands));
	auto List = R->List.Get();
	if (Frame.bTimeFrame)
	{
		BeginPassTiming(*R, Frame.TimingQueries);
	}
#if HYP_ENABLE_PROFILING
	BeginD3D12Profile(*R, P.Profiling, Frame.ProfileQueries);
#endif
	List->BeginEvent(1, InCommands.Name.c_str(), static_cast<UINT>(InCommands.Name.size() + 1));
	if (InCommands.TransitionFrom && InCommands.TransitionTo)
	{
		Transition(List, P.Backbuffers[P.FrameIndex].Get(), Native(*InCommands.TransitionFrom),
		           Native(*InCommands.TransitionTo));
	}
	for (const auto& Barrier : InCommands.TextureTransitions)
	{
		const auto& Texture = NativeResource<FD3D12Texture>(Barrier.Texture.Payload, P.State.get());
		Transition(List, Texture.Resource.Get(), Native(Barrier.Before), Native(Barrier.After));
	}
	auto Rtv = P.Rtvs->GetCPUDescriptorHandleForHeapStart();
	Rtv.ptr += std::size_t(P.FrameIndex + (InCommands.bSrgbTarget ? FrameCount : 0)) * P.RtvStep;
	auto Dsv = P.Dsvs->GetCPUDescriptorHandleForHeapStart();
	if (InCommands.DepthTarget)
	{
		Dsv = NativeResource<FD3D12Texture>(InCommands.DepthTarget.Payload, P.State.get())
		          .DepthViews->GetCPUDescriptorHandleForHeapStart();
	}
	RecordAttachments(*List, InCommands, TargetSize, Rtv, Dsv);
	if (DrawPlan)
	{
		RecordNativeDrawPlan(*List, *DrawPlan, *P.State);
	}
	else
	{
		RecordDraws(*List, InCommands, *P.State);
	}
	List->EndEvent();
#if HYP_ENABLE_PROFILING
	EndD3D12Profile(*R);
#endif
	EndPassTiming(*R);
	Check(List->Close(), "Close recording list");
	Frame.Recordings[InContext] = {R->List, R};
	return {std::move(R)};
}

FImage FD3D12RHISwapchain::EndFrame(std::span<const FRecordedList> InLists, bool bInVsync, bool bInCapture)
{
	HYP_PERF_SCOPE_C(Rhi, SubmitAndPresent);
	auto& P = *Impl;
	if (!P.bActive || InLists.empty())
	{
		throw std::logic_error("No active frame commands");
	}
	auto& Frame = P.Frames[P.FrameIndex];
	std::vector<ID3D12CommandList*> NativeLists;
	std::array<bool, ContextCount> Seen{};
	for (const auto& List : InLists)
	{
		const auto& NativeList = NativeResource<FD3D12RecordedList>(List.Payload, P.State.get());
		if (NativeList.Frame != P.Serial || NativeList.Owner != P.Identity || NativeList.Context >= ContextCount ||
		    Seen[NativeList.Context])
		{
			throw std::invalid_argument("Stale, foreign or duplicate command list");
		}
		Seen[NativeList.Context] = true;
		NativeLists.push_back(NativeList.List.Get());
	}
	Frame.Retained.assign(InLists.begin(), InLists.end());
	P.bSubmissionStarted = true;
	{
		HYP_PERF_SCOPE_C(Rhi, SubmitCommandLists);
		P.State->Queue->ExecuteCommandLists(static_cast<UINT>(NativeLists.size()), NativeLists.data());
	}
	FImage Result;
	if (bInCapture)
	{
		auto Desc = P.Backbuffers[P.FrameIndex]->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT Footprint{};
		UINT64 Total{};
		P.State->Device->GetCopyableFootprints(&Desc, 0, 1, 0, &Footprint, nullptr, nullptr, &Total);
		auto Readback = P.State->AllocateBuffer(Total, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
		P.State->Immediate(
		    [&](ID3D12GraphicsCommandList* InList)
		    {
			    auto Resource = P.Backbuffers[P.FrameIndex].Get();
			    Transition(InList, Resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE);
			    D3D12_TEXTURE_COPY_LOCATION Src{};
			    Src.pResource = Resource;
			    Src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			    D3D12_TEXTURE_COPY_LOCATION Dst{};
			    Dst.pResource = Readback->Resource.Get();
			    Dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			    Dst.PlacedFootprint = Footprint;
			    InList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
			    Transition(InList, Resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
		    },
		    {P.Backbuffers[P.FrameIndex], Readback->Resource}, {Readback->Allocation});
		Result = {P.Size.Width, P.Size.Height, EColorSpace::Srgb, {}};
		Result.Rgba.resize(std::size_t(P.Size.Width) * P.Size.Height * 4);
		void* Mapped{};
		D3D12_RANGE Range{0, static_cast<SIZE_T>(Total)};
		Check(Readback->Resource->Map(0, &Range, &Mapped), "Map screenshot");
		for (UINT Y = 0; Y < P.Size.Height; ++Y)
		{
			auto Row = static_cast<unsigned char*>(Mapped) + std::size_t(Y) * Footprint.Footprint.RowPitch;
			for (UINT X = 0; X < P.Size.Width * 4; ++X)
			{
				Result.Rgba[std::size_t(Y) * P.Size.Width * 4 + X] = Row[X] / 255.f;
			}
		}
		D3D12_RANGE Written{0, 0};
		Readback->Resource->Unmap(0, &Written);
	}
	HRESULT Hr{};
	{
		HYP_PERF_SCOPE_C(Rhi, PresentWait);
#if defined(HYP_TEST_D3D12_PRESENT)
		Hr = PresentForTesting(P.Swapchain.Get(), bInVsync ? 1 : 0);
#else
		Hr = P.Swapchain->Present(bInVsync ? 1 : 0, 0);
#endif
	}
	Frame.FenceValue = P.State->Signal();
	// Device-level retention also progresses when no further frame is presented.
	// Keep the frame copy until publishing the submission succeeds (including allocation failure).
	P.State->Submissions.push_back(
	    {Frame.FenceValue, Frame.Retained, P.State->GpuTimingCapacity ? P.State->GpuTimingEpoch : 0});
	Frame.Retained.clear();
	++P.State->Submitted;
	// A failed Present must remain cancellable until submitted work has drained.
	Check(Hr, "Present swapchain");
	P.bActive = false;
	return Result;
}

void FD3D12RHISwapchain::CancelFrame()
{
	auto& P = *Impl;
	if (!P.bActive)
	{
		return;
	}
	if (P.bSubmissionStarted)
	{
		// Keep Active and retained resources intact if draining the device fails.
		P.Idle();
	}
	P.Frames[P.FrameIndex].Retained.clear();
	P.bActive = false;
}

void FD3D12RHISwapchain::WaitIdle()
{
	Impl->Idle();
}

void FD3D12RHISwapchain::SetGpuTimingEnabled(bool bInEnabled)
{
	if (Impl->bActive)
	{
		throw std::logic_error("GPU timing cannot change during an active frame");
	}
	Impl->bGpuTiming = bInEnabled;
}

} // namespace Hyperion
