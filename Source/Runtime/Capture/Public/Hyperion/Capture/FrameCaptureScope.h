#pragma once
#include "Hyperion/Capture/FrameCapture.h"

namespace Hyperion
{
class FTaskSystem;

// Stack-owned on RHI 0, bracketing preparation, recording, submission and Present.
// The service and task system must outlive the scope. Only this request can open replay.
class FFrameCaptureScope
{
public:
	FFrameCaptureScope(FTaskSystem& InTasks, FFrameCapture* InCapture, FNativeSurface InSurface, bool bInRequest);
	~FFrameCaptureScope();
	FFrameCaptureScope(const FFrameCaptureScope&) = delete;
	FFrameCaptureScope& operator=(const FFrameCaptureScope&) = delete;
	bool IsAccepted() const;
	bool Finish(bool bInOpenReplay);

private:
	FFrameCapture* Capture{};
	bool bAccepted{};
	bool bActive{};
};
} // namespace Hyperion
