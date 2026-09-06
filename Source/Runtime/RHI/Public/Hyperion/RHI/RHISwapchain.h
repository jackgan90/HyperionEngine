#pragma once
#include "Hyperion/Platform/Window.h"
#include "Hyperion/RHI/RHICapabilities.h"
#include "Hyperion/RHI/RHITypes.h"
#include <span>

namespace Hyperion
{
struct FRHISwapchainDesc
{
	FNativeSurface Surface;
	FSize Size;
};

// Current presentation contract records passes against one swapchain color target.
// Frame control/submission/destruction belongs to RHI 0. Distinct Record contexts
// may run concurrently. This is not yet a general offscreen command interface.
class IRHISwapchain
{
public:
	virtual ~IRHISwapchain() = default;
	virtual const FRHICapabilities& GetCapabilities() const noexcept = 0;
	virtual void BeginFrame(FSize InSize) = 0;
	virtual FRecordedList Record(std::uint32_t InContext, const FPassCommands& InCommands) = 0;
	virtual FImage EndFrame(std::span<const FRecordedList> InLists, bool InVsync, bool InCapture = false) = 0;
	// Coordinator only, after all recorders finish. Idempotent; submitted work must
	// finish before releasing resources. Throws if the device cannot recover safely.
	virtual void CancelFrame() = 0;
	virtual void WaitIdle() = 0;
};
} // namespace Hyperion
