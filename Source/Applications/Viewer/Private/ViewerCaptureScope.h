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
	bool Active = false;

	FFrameCaptureScope(FTaskSystem& InTasks, FFrameCapture* InCapture, FNativeSurface InSurface)
	    : Tasks(InTasks), Capture(InCapture)
	{
		if (Capture)
		{
			Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                          [&]
			                          {
				                          Active = Capture->BeginFrame(InSurface);
			                          }));
		}
	}

	~FFrameCaptureScope()
	{
		if (Active)
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
		bool Success = false;
		if (Active)
		{
			Tasks.Wait(Tasks.Dispatch({EDomain::Rhi, 0},
			                          [&]
			                          {
				                          Success = Capture->EndFrame();
			                          }));
			Active = false;
		}
		return Success;
	}
};
} // namespace Hyperion
#endif
