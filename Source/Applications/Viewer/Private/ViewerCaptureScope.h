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
		if (Capture)
		{
			Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                          [&]
			                          {
				                          bActive = Capture->BeginFrame(InSurface);
			                          }));
		}
	}

	~FFrameCaptureScope()
	{
		if (bActive)
		{
			try
			{
				Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
				                          [&]
				                          {
					                          Capture->Cancel();
				                          }));
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
			Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                          [&]
			                          {
				                          bSuccess = Capture->EndFrame();
			                          }));
			bActive = false;
		}
		return bSuccess;
	}
};
} // namespace Hyperion
#endif
