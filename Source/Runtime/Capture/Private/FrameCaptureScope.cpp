#include "Hyperion/Capture/FrameCaptureScope.h"
#include "Hyperion/Tasks/TaskSystem.h"

namespace Hyperion
{
FFrameCaptureScope::FFrameCaptureScope(FTaskSystem& InTasks, FFrameCapture* InCapture, FNativeSurface InSurface,
                                       bool bInRequest)
    : Capture(InCapture)
{
	InTasks.Require({EDomain::Rhi, 0});
	if (Capture && bInRequest)
	{
		bAccepted = Capture->RequestCapture();
		if (bAccepted)
		{
			bActive = Capture->BeginFrame(InSurface);
		}
	}
}

FFrameCaptureScope::~FFrameCaptureScope()
{
	if (bActive)
	{
		Capture->Cancel();
	}
}

bool FFrameCaptureScope::IsAccepted() const
{
	return bAccepted;
}

bool FFrameCaptureScope::Finish(bool bInOpenReplay)
{
	if (!bActive)
	{
		return false;
	}
	const bool bSuccess = Capture->EndFrame();
	bActive = false;
	if (bSuccess && bInOpenReplay)
	{
		// Open before a later queued frame can replace LastCapture.
		Capture->OpenLastCapture();
	}
	return bSuccess;
}
} // namespace Hyperion
