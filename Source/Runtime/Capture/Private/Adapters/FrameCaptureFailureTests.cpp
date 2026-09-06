// Test-only executable source: vendor fault injection stays inside the private adapter boundary.
#include "Hyperion/Capture/FrameCapture.h"
#include <Windows.h>
#include <iostream>
#include <renderdoc_app.h>
#include <source_location>
#include <stdexcept>

namespace
{
bool bCapturing = false;
bool bStopsOnFailure = false;
std::uint32_t DiscardCalls = 0;

void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Capture failure recovery check failed at line " + std::to_string(InLocation.line()));
	}
}

void RENDERDOC_CC Start(RENDERDOC_DevicePointer, RENDERDOC_WindowHandle)
{
	bCapturing = true;
}

std::uint32_t RENDERDOC_CC IsCapturing()
{
	return bCapturing;
}

std::uint32_t RENDERDOC_CC End(RENDERDOC_DevicePointer, RENDERDOC_WindowHandle)
{
	return 0;
}

std::uint32_t RENDERDOC_CC Discard(RENDERDOC_DevicePointer, RENDERDOC_WindowHandle)
{
	if (++DiscardCalls == 1)
	{
		bCapturing = !bStopsOnFailure;
		return 0;
	}
	bCapturing = false;
	return 1;
}

struct FScopedFailure
{
	RENDERDOC_API_1_6_0& Api;
	RENDERDOC_API_1_6_0 Original;

	explicit FScopedFailure(RENDERDOC_API_1_6_0& InApi) : Api(InApi), Original(InApi)
	{
		bCapturing = false;
		DiscardCalls = 0;
		Api.StartFrameCapture = Start;
		Api.IsFrameCapturing = IsCapturing;
		Api.EndFrameCapture = End;
		Api.DiscardFrameCapture = Discard;
	}

	~FScopedFailure()
	{
		// The dedicated process creates no GPU objects. Restore its API table even after a failed assertion.
		Api = Original;
	}
};
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		for (int Case = 0; Case < 4; ++Case)
		{
			FFrameCapture Capture({{}, "capture-failure-tests", "Failure"});
			Capture.Initialize();
			if (!Capture.Status().bAvailable)
			{
				std::cout << "SKIP: " << Capture.Status().Message << '\n';
				return 77;
			}
			const auto GetApi = reinterpret_cast<pRENDERDOC_GetAPI>(
			    GetProcAddress(GetModuleHandleW(L"renderdoc.dll"), "RENDERDOC_GetAPI"));
			RENDERDOC_API_1_6_0* Api = nullptr;
			Check(GetApi && GetApi(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&Api)) && Api);
			FScopedFailure Failure(*Api);
			bStopsOnFailure = Case == 3;
			Check(Capture.RequestCapture());
			Check(Capture.BeginFrame({}));
			if (Case == 1)
			{
				Capture.Shutdown();
			}
			else if (Case == 2)
			{
				Check(!Capture.EndFrame());
			}
			else
			{
				Capture.Cancel();
			}
			Check(DiscardCalls == 1);
			Check(Capture.Status().CompletedCaptures == 0 && Capture.Status().LastCapture.empty());
			if (bStopsOnFailure)
			{
				Check(!bCapturing && Capture.Status().bAvailable);
				Check(Capture.RequestCapture());
				Capture.Cancel();
				Check(DiscardCalls == 1);
				continue;
			}
			Check(bCapturing && !Capture.Status().bAvailable);
			Check(Capture.Status().State == EFrameCaptureState::Failed);
			Check(Capture.Status().Message.find("still active") != std::string::npos);
			Capture.Initialize();
			Check(!Capture.Status().bAvailable && !Capture.RequestCapture());
			if (Case == 1)
			{
				Capture.Shutdown();
				Check(Capture.Status().State == EFrameCaptureState::Disabled);
			}
			else
			{
				Capture.Cancel();
			}
			Check(DiscardCalls == 2 && !bCapturing);
			Check(!Capture.Status().bAvailable);
		}
		std::cout << "Cancel/End/Shutdown failure ownership, retry and inactive-error recovery passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
