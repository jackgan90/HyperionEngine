#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/RHI/RHIPipeline.h"
#include "Support/ShaderSourceSupport.h"
#include "Support/TestSupport.h"
#include <cstring>
#include <fstream>
#include <iostream>

namespace
{
using namespace Hyperion;

template<typename TOperation> void Rejects(TOperation InOperation)
{
	bool bRejected = false;
	try
	{
		InOperation();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

struct FParameters
{
	std::uint32_t Width;
	std::uint32_t Height;
	float Scale;
	float Add;
};

void CheckConstantResetRejected(IRHIDevice& InDevice, const FPassCommands& InCommands)
{
	for (const auto& Dispatch : InCommands.Dispatches)
	{
		const auto& Page = Dispatch.ConstantBindings[0].Slice.Buffer;
		HYP_CHECK(Page.Payload.use_count() == 1);
		Rejects(
		    [&]
		    {
			    InDevice.ResetConstantBuffer(Page);
		    });
	}
}

void CheckRetainedComputeOwnership(IRHIDevice& InDevice, IRHISwapchain& InSwapchain, FSize InSize,
                                   FPassCommands InCommands)
{
	InCommands.Dispatches.push_back(InCommands.Dispatches.front());
	for (std::size_t Index = 0; Index < InCommands.Dispatches.size(); ++Index)
	{
		const FParameters Parameters{19, 7, 4, 20 + float(Index)};
		auto Page = InDevice.CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
		InCommands.Dispatches[Index].ConstantBindings = {
		    {0, InDevice.PublishConstantSlice(Page, 0, std::as_bytes(std::span(&Parameters, 1)))}};
	}
	FPassCommands Finish;
	Finish.Transitions = InCommands.Transitions;
	for (auto& Barrier : Finish.Transitions)
	{
		std::swap(Barrier.Before, Barrier.After);
	}
	auto Owned = std::make_shared<const FPassCommands>(std::move(InCommands));
	const std::weak_ptr<const FPassCommands> WeakOwner = Owned;
	std::vector<FRecordedList> Lists;
	for (unsigned Mode = 0; Mode < 3; ++Mode)
	{
		InSwapchain.BeginFrame(InSize);
		if (Mode == 2)
		{
			InSwapchain.PrepareFrameRecording(3);
			const std::array Passes{std::make_shared<const FPassCommands>(), Owned,
			                        std::make_shared<const FPassCommands>(Finish)};
			Lists = {InSwapchain.RecordBatchOwned(0, Passes, 0)};
		}
		else
		{
			Lists = {InSwapchain.RecordOwned(0, Owned), InSwapchain.Record(1, Finish)};
		}
		CheckConstantResetRejected(InDevice, *Owned);
		if (Mode == 0)
		{
			InSwapchain.CancelFrame();
		}
		else
		{
			InSwapchain.EndFrame(Lists, false);
		}
		CheckConstantResetRejected(InDevice, *Owned);
		if (Mode != 2)
		{
			InSwapchain.WaitIdle();
			CheckConstantResetRejected(InDevice, *Owned);
		}
	}
	const auto Output = Owned->TextureAccesses.front().View;
	Owned.reset();
	Lists.clear();
	std::vector<FBuffer> Pages;
	{
		const auto Submitted = WeakOwner.lock();
		HYP_CHECK(Submitted); // Only the ordinary submission retains the command shell now.
		CheckConstantResetRejected(InDevice, *Submitted);
		for (const auto& Dispatch : Submitted->Dispatches)
		{
			Pages.push_back(Dispatch.ConstantBindings[0].Slice.Buffer);
		}
	}
	InSwapchain.WaitIdle();
	HYP_CHECK(WeakOwner.expired());
	for (const auto& Page : Pages)
	{
		HYP_CHECK(Page.Payload.use_count() == 1);
		InDevice.ResetConstantBuffer(Page);
	}
	const auto Data = InDevice.ReadTexture(Output, EResourceState::ShaderRead);
	for (std::size_t Index = 0; Index < Data.size() / sizeof(float); ++Index)
	{
		float Actual{};
		std::memcpy(&Actual, Data.data() + Index * sizeof(float), sizeof(float));
		HYP_CHECK(Actual == float(Index) * 4 + 21);
	}
}

void CheckInvalidDispatches(IRHISwapchain& InSwapchain, const FPassCommands& InCommands)
{
	auto Invalid = InCommands;
	Invalid.Dispatches[0].Groups[0] = 0;
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, Invalid);
	    });
	Invalid = InCommands;
	Invalid.TextureAccesses.clear();
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, Invalid);
	    });
	Invalid = InCommands;
	Invalid.TextureAccesses.push_back(
	    {{InCommands.TextureAccesses.front().View.Texture, 1, 1}, EResourceState::ShaderRead});
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, Invalid);
	    });
	Invalid = InCommands;
	Invalid.TextureAccesses[0].View.MipCount = UINT32_MAX;
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, Invalid);
	    });
	Invalid = InCommands;
	Invalid.BufferAccesses[0].View.Offset = UINT64_MAX;
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, Invalid);
	    });
	Invalid = InCommands;
	Invalid.BufferAccesses.push_back({InCommands.BufferAccesses.front().View, EResourceState::ShaderRead});
	Rejects(
	    [&]
	    {
		    InSwapchain.Record(0, Invalid);
	    });
}

std::filesystem::path WriteComputeFixture()
{
	const auto Root = TestShaderRoot();
	std::ofstream(Root / "Compute.hlsl") << R"(
cbuffer Parameters : register(b2, space1) { uint Width; uint Height; float Scale; float Add; };
RWTexture2D<float> Output : register(u0);
RWStructuredBuffer<float> Values : register(u1);
RWByteAddressBuffer Raw : register(u2);
[numthreads(8,4,1)] void Main(uint3 Id : SV_DispatchThreadID)
{
    if (Id.x >= Width || Id.y >= Height) return;
    uint Index = Id.y * Width + Id.x;
    float Value = Index * Scale + Add;
    Output[Id.xy] = Value;
    Values[Index] = Value + 1;
    Raw.Store(Index * 4, asuint(Value + 2));
})";
	return Root;
}

void SubmitCompute(IRHISwapchain& InSwapchain, FSize InSize, const FPassCommands& InCommands, bool bInBatch)
{
	InSwapchain.BeginFrame(InSize);
	CheckInvalidDispatches(InSwapchain, InCommands);
	FPassCommands Finish;
	Finish.Name = "Compute export";
	Finish.Transitions = InCommands.Transitions;
	for (auto& Barrier : Finish.Transitions)
	{
		std::swap(Barrier.Before, Barrier.After);
	}
	if (!bInBatch)
	{
		const std::array Lists{InSwapchain.Record(0, InCommands), InSwapchain.Record(1, Finish)};
		InSwapchain.EndFrame(Lists, false);
		return;
	}
	InSwapchain.PrepareFrameRecording(25);
	std::vector<std::shared_ptr<const FPassCommands>> Passes;
	Passes.push_back(std::make_shared<const FPassCommands>(InCommands));
	for (std::uint32_t Index = 1; Index < 24; ++Index)
	{
		auto Next = InCommands;
		Next.Name = "Dependent compute " + std::to_string(Index);
		for (auto& Barrier : Next.Transitions)
		{
			Barrier.Before = EResourceState::ShaderWrite;
			Barrier.bUavBarrier = true;
		}
		Passes.push_back(std::make_shared<const FPassCommands>(std::move(Next)));
	}
	Passes.push_back(std::make_shared<const FPassCommands>(Finish));
	const std::array Lists{InSwapchain.RecordBatchOwned(0, std::span(Passes).first(12), 0),
	                       InSwapchain.RecordBatchOwned(1, std::span(Passes).subspan(12), 12)};
	const auto Overlap = InSwapchain.RecordBatchOwned(2, std::span(Passes).first(1), 0);
	const std::array InvalidLists{Lists[0], Lists[1], Overlap};
	Rejects(
	    [&]
	    {
		    InSwapchain.EndFrame(InvalidLists, false);
	    });
	InSwapchain.EndFrame(Lists, false);
}

void RunCompute()
{
	const auto Root = WriteComputeFixture();
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	auto Device = Registry.CreateDevice(ERHIBackend::D3D12, {});
	FShaderCompiler Compiler(Root, "compute-rhi-cache");
	auto Shader = Compiler.Compile("Compute.hlsl", "Main", EShaderStage::Compute, EShaderFormat::Dxil);
	auto Layout = Device->CreateBindingLayout(
	    {{{ERHIBindingKind::ConstantBuffer, ERHIShaderVisibility::Compute, 2, 1, 1, 16},
	      {ERHIBindingKind::StorageTexture2D, ERHIShaderVisibility::Compute, 0},
	      {ERHIBindingKind::StorageStructuredBuffer, ERHIShaderVisibility::Compute, 1, 0, 1, 0, 4},
	      {ERHIBindingKind::StorageRawBuffer, ERHIShaderVisibility::Compute, 2}}});
	auto Pipeline = Device->CreateComputePipeline({Shader, Layout});
	constexpr std::uint32_t Width = 19;
	constexpr std::uint32_t Height = 7;
	const auto Texture = Device->CreateStorageTexture({Width * 2 + 1, Height * 2 + 1, 2, ERHIColorFormat::R32Float});
	const FTextureView View{Texture, 1, 1};
	const auto Values = Device->CreateBuffer({Width * Height * 4, BufferUsage(ERHIBufferUsage::StructuredWrite) |
	                                                                  BufferUsage(ERHIBufferUsage::StructuredRead)});
	const auto Raw = Device->CreateBuffer(
	    {Width * Height * 4, BufferUsage(ERHIBufferUsage::RawWrite) | BufferUsage(ERHIBufferUsage::RawRead)});
	const FReadBufferView ValueView{Values, ERHIBufferViewKind::Structured, 0, Width * Height * 4, 4};
	const FReadBufferView RawView{Raw, ERHIBufferViewKind::Raw, 0, Width * Height * 4, 0};
	const auto Bindings = Device->CreateBindingSet({Layout, {{1, {View}}, {2, {ValueView}}, {3, {RawView}}}});
	Rejects(
	    [&]
	    {
		    Device->CreateBindingSet({Layout, {{1, {FTextureView{Texture, 0, 2}}}, {2, {ValueView}}, {3, {RawView}}}});
	    });
	FWindow Window("Compute validation", {64, 64}, true);
	auto Swapchain = Device->CreateSwapchain({Window.Surface(), Window.PixelSize()});
	Swapchain->SetGpuTimingEnabled(true);
	for (std::uint32_t Frame = 0; Frame < 2; ++Frame)
	{
		const FParameters Parameters{Width, Height, 2.0f + Frame, 10.0f + Frame};
		auto Page = Device->CreateBuffer({256, BufferUsage(ERHIBufferUsage::Constant)});
		const auto Constants = Device->PublishConstantSlice(Page, 0, std::as_bytes(std::span(&Parameters, 1)));
		FPassCommands Commands;
		Commands.Name = "Compute validation";
		Commands.bCompute = true;
		Commands.Dispatches = {{Pipeline,
		                        Bindings,
		                        {{0, Constants}},
		                        ComputeDispatchGroups({Width, Height, 1}, Shader.Reflection.ThreadGroupSize)}};
		Commands.TextureAccesses = {{View, EResourceState::ShaderWrite}};
		Commands.BufferAccesses = {{ValueView, EResourceState::ShaderWrite}, {RawView, EResourceState::ShaderWrite}};
		Commands.Transitions = {
		    {FRenderTarget::FromTexture(Texture), EResourceState::ShaderRead, EResourceState::ShaderWrite, 1, 1},
		    {{}, EResourceState::ShaderRead, EResourceState::ShaderWrite, 0, 0, Values},
		    {{}, EResourceState::ShaderRead, EResourceState::ShaderWrite, 0, 0, Raw}};
		if (Frame == 0)
		{
			CheckRetainedComputeOwnership(*Device, *Swapchain, Window.PixelSize(), Commands);
		}
		SubmitCompute(*Swapchain, Window.PixelSize(), Commands, Frame != 0);
		const std::array Results{Device->ReadTexture(View, EResourceState::ShaderRead),
		                         Device->ReadBuffer(ValueView, EResourceState::ShaderRead),
		                         Device->ReadBuffer(RawView, EResourceState::ShaderRead)};
		for (std::size_t Resource = 0; Resource < Results.size(); ++Resource)
		{
			HYP_CHECK(Results[Resource].size() == Width * Height * 4);
			for (std::uint32_t Index = 0; Index < Width * Height; ++Index)
			{
				float Value{};
				std::memcpy(&Value, Results[Resource].data() + Index * 4, 4);
				HYP_CHECK(Value == Index * Parameters.Scale + Parameters.Add + Resource);
			}
		}
	}
	Device->WaitIdle();
	HYP_CHECK(Device->Statistics().GpuTiming.Passes.size() == 25);
	HYP_CHECK(Device->Statistics().ValidationErrors == 0);
}
} // namespace

int main()
{
	try
	{
		RunCompute();
		std::cout << "Compute texture, structured/raw buffers, parameters and mip readback passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
