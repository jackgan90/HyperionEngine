#pragma once
#include "Hyperion/Platform/Window.h"
#include <filesystem>
#include <memory>
#include <string>

namespace Hyperion
{
enum class EFrameCaptureState
{
	Disabled,
	Unavailable,
	Ready,
	Pending,
	Capturing,
	Succeeded,
	Failed
};

struct FFrameCaptureSettings
{
	std::filesystem::path LibraryPath;
	std::filesystem::path OutputDirectory;
	std::string Prefix = "Hyperion";
};

struct FFrameCaptureStatus
{
	EFrameCaptureState State = EFrameCaptureState::Disabled;
	bool bAvailable = false;
	std::string Message = "RenderDoc is disabled (restart to enable)";
	std::filesystem::path LastCapture;
	std::uint64_t CompletedCaptures = 0;
	std::uint32_t ReplayProcessId = 0;
	std::string ReplayMessage;
};

// Initialize before the first graphics API call. Begin/End/Cancel belong to RHI 0
// and bracket all frame preparation, recording and presentation. Other methods
// are synchronized, but callers must not destroy this service during a frame.
// Initial implementation targets one graphics device and one native window.
class FFrameCapture
{
public:
	explicit FFrameCapture(FFrameCaptureSettings InSettings);
	~FFrameCapture();
	FFrameCapture(const FFrameCapture&) = delete;
	FFrameCapture& operator=(const FFrameCapture&) = delete;
	void Initialize();
	void Shutdown() noexcept;
	FFrameCaptureStatus Status() const;
	bool RequestCapture();
	bool BeginFrame(FNativeSurface InSurface);
	bool EndFrame();
	void Cancel() noexcept;
	bool OpenLastCapture();

private:
	struct FImpl;
	std::unique_ptr<FImpl> Impl;
};
} // namespace Hyperion
