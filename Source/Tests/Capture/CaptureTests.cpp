#include "Hyperion/Capture/FrameCapture.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "Hyperion/Renderer/RenderGraph.h"
#include "Support/TestSupport.h"
#include <fstream>
#include <iostream>

namespace
{
using namespace Hyperion;

void Unavailable(const std::filesystem::path& InLibrary)
{
	FFrameCapture Capture({InLibrary, "capture-unavailable", "Unavailable"});
	HYP_CHECK(Capture.Status().State == EFrameCaptureState::Disabled);
	HYP_CHECK(!Capture.RequestCapture());
	Capture.Initialize();
	HYP_CHECK(Capture.Status().State == EFrameCaptureState::Unavailable);
	HYP_CHECK(!Capture.Status().Available);
	HYP_CHECK(!Capture.RequestCapture());
	HYP_CHECK(!Capture.BeginFrame({}));
	HYP_CHECK(!Capture.EndFrame());
	HYP_CHECK(!Capture.OpenLastCapture());
	HYP_CHECK(!Capture.Status().ReplayMessage.empty());
	Capture.Cancel();
	Capture.Shutdown();
	Capture.Shutdown();
	HYP_CHECK(Capture.Status().State == EFrameCaptureState::Disabled);
}

int Runtime()
{
	const auto Root = std::filesystem::absolute("capture-service-tests") / std::to_string(ClockNanoseconds());
	FFrameCapture Capture({{}, Root / std::filesystem::path(u8"RDC 测试"), "Service"});
	Capture.Initialize();
	if (!Capture.Status().Available)
	{
		std::cout << "SKIP: " << Capture.Status().Message << '\n';
		return 77;
	}
	HYP_CHECK(Capture.RequestCapture());
	HYP_CHECK(!Capture.RequestCapture());
	HYP_CHECK(!Capture.BeginFrame({})); // No graphics API has initialized yet.
	HYP_CHECK(Capture.Status().State == EFrameCaptureState::Failed);
	HYP_CHECK(Capture.Status().LastCapture.empty());
	HYP_CHECK(Capture.RequestCapture());
	Capture.Cancel();
	HYP_CHECK(!Capture.EndFrame());
	HYP_CHECK(Capture.Status().State == EFrameCaptureState::Failed);

	// A second service represents an external capture owner for exclusivity tests.
	FFrameCapture External({{}, Root / "External", "External"});
	External.Initialize();
	FTaskSystem Tasks(1, 2);
	FWindow Window("Capture service acceptance", {128, 128}, true);
	const auto Surface = Window.Surface();
	FRHIBackendRegistry Registry;
	RegisterD3D12RHIBackend(Registry);
	std::unique_ptr<IRHIDevice> Device;
	std::unique_ptr<IRHISwapchain> Swapchain;
	auto Rhi = [&](auto InWork)
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0}, InWork));
	};
	Rhi(
	    [&]
	    {
		    Device = Registry.CreateDevice(ERHIBackend::D3D12);
		    Swapchain = Device->CreateSwapchain({Surface, {128, 128}});
	    });
	auto Draw = [&]
	{
		Tasks.Wait(Tasks.Dispatch({EDomain::Render},
		                          [&]
		                          {
			                          FRenderGraph Graph;
			                          FColorPass Pass;
			                          Pass.Commands.Name = "Capture service clear";
			                          Pass.Commands.ClearColor = {.2f, .4f, .8f, 1};
			                          Pass.Load = EColorLoad::Clear;
			                          Graph.Add(std::move(Pass));
			                          ExecuteGraph(Graph, Tasks, *Swapchain, {128, 128}, false, false);
		                          }));
	};
	HYP_CHECK(Capture.RequestCapture());
	Rhi(
	    [&]
	    {
		    HYP_CHECK(Capture.BeginFrame(Surface));
	    });
	HYP_CHECK(!External.RequestCapture());
	External.Cancel(); // Must not discard Capture's active capture.
	Draw();
	Rhi(
	    [&]
	    {
		    HYP_CHECK(Capture.EndFrame());
	    });
	const auto First = Capture.Status().LastCapture;
	HYP_CHECK(std::filesystem::file_size(First) > 0);
	HYP_CHECK(Capture.Status().CompletedCaptures == 1);

	HYP_CHECK(Capture.RequestCapture());
	HYP_CHECK(External.RequestCapture());
	Rhi(
	    [&]
	    {
		    HYP_CHECK(External.BeginFrame(Surface));
	    });
	Rhi(
	    [&]
	    {
		    HYP_CHECK(!Capture.BeginFrame(Surface));
	    });
	HYP_CHECK(Capture.Status().State == EFrameCaptureState::Failed);
	HYP_CHECK(Capture.Status().LastCapture == First);
	HYP_CHECK(Capture.Status().CompletedCaptures == 1);
	HYP_CHECK(!Capture.EndFrame());
	Capture.Cancel();
	Draw();
	Rhi(
	    [&]
	    {
		    HYP_CHECK(External.EndFrame());
	    });

	HYP_CHECK(Capture.RequestCapture());
	Rhi(
	    [&]
	    {
		    HYP_CHECK(Capture.BeginFrame(Surface));
		    Capture.Cancel();
	    });
	HYP_CHECK(Capture.Status().CompletedCaptures == 1);
	HYP_CHECK(Capture.RequestCapture());
	Rhi(
	    [&]
	    {
		    HYP_CHECK(Capture.BeginFrame(Surface));
	    });
	Draw();
	Rhi(
	    [&]
	    {
		    HYP_CHECK(Capture.EndFrame());
	    });
	HYP_CHECK(Capture.Status().CompletedCaptures == 2);
	HYP_CHECK(Capture.Status().LastCapture != First);

	const auto Blocker = Root / "OutputIsAFile";
	std::ofstream(Blocker) << "sentinel";
	FFrameCapture BadOutput({{}, Blocker / "child", "Failure"});
	BadOutput.Initialize();
	HYP_CHECK(BadOutput.RequestCapture());
	Rhi(
	    [&]
	    {
		    HYP_CHECK(!BadOutput.BeginFrame(Surface));
	    });
	HYP_CHECK(BadOutput.Status().State == EFrameCaptureState::Failed);
	HYP_CHECK(BadOutput.Status().CompletedCaptures == 0);
	HYP_CHECK(BadOutput.Status().LastCapture.empty());
	HYP_CHECK(std::filesystem::file_size(Blocker) == 8);
	// Remove only this test-owned file to verify stale paths cannot launch the UI.
	std::filesystem::remove(Capture.Status().LastCapture);
	HYP_CHECK(!Capture.OpenLastCapture());
	HYP_CHECK(Capture.Status().State == EFrameCaptureState::Succeeded);
	HYP_CHECK(Capture.Status().CompletedCaptures == 2);
	HYP_CHECK(Capture.Status().ReplayProcessId == 0);
	Rhi(
	    [&]
	    {
		    Device->WaitIdle();
		    HYP_CHECK(Device->Statistics().ValidationErrors == 0);
		    Capture.Shutdown();
		    External.Shutdown();
		    Swapchain.reset();
		    Device.reset();
	    });
	std::cout << "Capture ownership, cancellation, retries, Unicode paths and verified results passed\n";
	return 0;
}
} // namespace

int main(int InArgc, char** InArgv)
{
	try
	{
		if (InArgc > 1 && std::string(InArgv[1]) == "--runtime")
		{
			return Runtime();
		}
		Unavailable(std::filesystem::absolute("missing-renderdoc-runtime.dll"));
		if (InArgc > 1)
		{
			Unavailable(InArgv[1]);
		}
		std::cout << "Unavailable runtime, incompatible API and disabled operations passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
