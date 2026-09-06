// Native fault injection stays inside the backend and is compiled only into this test executable.
#include "D3D12Resources.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include <chrono>
#include <iostream>
#include <source_location>
#include <thread>

namespace Hyperion
{
namespace
{
bool bFailPresent = false;

void CheckCondition(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Frame failure check failed at line " + std::to_string(InLocation.line()));
	}
}

struct FFrameFixture
{
	FTaskSystem Tasks{1, 2};
	FWindow Window{"Submitted frame failure", {64, 64}, true};
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	std::shared_ptr<FD3D12DeviceState> State;

	FFrameFixture()
	{
		const auto Surface = Window.Surface();
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          FRHIBackendRegistry Registry;
			                          RegisterD3D12RHIBackend(Registry);
			                          Device = Registry.CreateDevice(ERHIBackend::D3D12);
			                          Swapchain = Device->CreateSwapchain({Surface, {64, 64}});
			                          const std::array<std::byte, 4> Bytes{};
			                          auto Buffer = Device->CreateBuffer(Bytes);
			                          State = std::dynamic_pointer_cast<FD3D12Buffer>(Buffer.Payload)->State;
		                          }));
	}

	~FFrameFixture()
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
		                          [&]
		                          {
			                          Device->WaitIdle();
			                          Swapchain.reset();
			                          Device.reset();
			                          State.reset();
		                          }));
	}
};

FRenderGraph ClearGraph()
{
	FRenderGraph Graph;
	FColorPass Clear;
	Clear.Commands.Name = "Clear";
	Clear.Load = EColorLoad::Clear;
	Clear.Commands.ClearColor = {.25f, .5f, .75f, 1};
	Graph.Add(Clear);
	return Graph;
}

void CheckSubmittedFailure(FFrameFixture& InFixture, const FRenderGraph& InGraph)
{
	auto& Tasks = InFixture.Tasks;
	auto& State = *InFixture.State;
	ComPtr<ID3D12Fence> Gate;
	Check(State.Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Gate)), "Create test gate");
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          Check(State.Queue->Wait(Gate.Get(), 1), "Block test queue");
	                          }));
	// A healthy queue is deliberately held back; no driver or device-loss fault is induced.
	std::jthread ReleaseGate(
	    [Gate]
	    {
		    std::this_thread::sleep_for(std::chrono::milliseconds(500));
		    Gate->Signal(1);
	    });
	bFailPresent = true;
	bool bFailed = false;
	bool bDrained = false;
	try
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          ExecuteGraph(InGraph, Tasks, *InFixture.Swapchain, {64, 64}, false, false);
		                          }));
	}
	catch (const std::exception& Error)
	{
		bFailed = std::string(Error.what()).find("Present swapchain (HRESULT 0x80004005)") != std::string::npos;
		bDrained = Gate->GetCompletedValue() >= 1 && State.Fence->GetCompletedValue() >= State.NextFence - 1;
	}
	ReleaseGate.join(); // Always unblock the queue before assertions or fixture destruction.
	CheckCondition(bFailed);
	CheckCondition(bDrained);
}

void CheckRecovery(FFrameFixture& InFixture, const FRenderGraph& InGraph)
{
	auto& Tasks = InFixture.Tasks;
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          InFixture.Swapchain->CancelFrame();
		                          InFixture.Swapchain->CancelFrame();
	                          }));
	Tasks.Wait(Tasks.Dispatch({EDomain::Render},
	                          [&]
	                          {
		                          const auto Image =
		                              ExecuteGraph(InGraph, Tasks, *InFixture.Swapchain, {64, 64}, false, true);
		                          CheckCondition(Image.Width == 64 && std::abs(Image.Rgba[0] - .25f) < .01f);
	                          }));
	Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
	                          [&]
	                          {
		                          InFixture.Device->WaitIdle();
		                          CheckCondition(InFixture.Device->Statistics().ValidationErrors == 0);
	                          }));
}
} // namespace

HRESULT PresentForTesting(IDXGISwapChain3* InSwapchain, UINT InInterval)
{
	if (bFailPresent)
	{
		bFailPresent = false;
		return E_FAIL;
	}
	return InSwapchain->Present(InInterval, 0);
}
} // namespace Hyperion

int main()
{
	try
	{
		Hyperion::FFrameFixture Fixture;
		const auto Graph = Hyperion::ClearGraph();
		Hyperion::CheckSubmittedFailure(Fixture, Graph);
		Hyperion::CheckRecovery(Fixture, Graph);
		std::cout << "Submitted frame failure drains the GPU and permits recovery\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
