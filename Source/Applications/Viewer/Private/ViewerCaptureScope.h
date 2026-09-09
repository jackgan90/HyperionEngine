#pragma once
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/Capture/FrameCapture.h"
#include "Hyperion/Tasks/TaskSystem.h"

namespace Hyperion
{
struct FFrameCaptureScope
{
	FTaskSystem& Tasks;
	FFrameCapture* Capture;
	bool bActive = false;

	FFrameCaptureScope(FTaskSystem& InTasks, FFrameCapture* InCapture, FNativeSurface InSurface)
	    : Tasks(InTasks), Capture(InCapture)
	{
		Tasks.Require({EDomain::Rhi, 0});
		if (Capture)
		{
			bActive = Capture->BeginFrame(InSurface);
		}
	}

	~FFrameCaptureScope()
	{
		if (bActive)
		{
			try
			{
				Capture->Cancel();
			}
			catch (...)
			{
			}
		}
	}

	bool Finish()
	{
		bool bSuccess = false;
		if (bActive)
		{
			bSuccess = Capture->EndFrame();
			bActive = false;
		}
		return bSuccess;
	}
};
} // namespace Hyperion
#endif
