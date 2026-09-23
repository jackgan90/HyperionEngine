#pragma once
#include "Hyperion/Reflection/RecordValue.h"

namespace Hyperion
{
struct FRenderCaptureInfo
{
	bool bCompiled{};
	bool bAvailable{};
	bool bBusy{};
	bool bRunning{};
	bool bFailed{};
	std::optional<bool> Preference;
	std::uint64_t Completed{};
	std::string Path;
	std::string Message;
	std::string ReplayMessage;
};

class IRenderCaptureControl
{
public:
	virtual ~IRenderCaptureControl() = default;
	virtual FRenderCaptureInfo RenderCaptureInfo() const = 0;
	virtual void RequestRenderCapture() = 0;
	virtual void OpenRenderCapture() = 0;
	virtual void SetRenderCapturePreference(bool bInEnabled);
};

template<> const FRecordDescriptor& RecordType<FRenderCaptureInfo>();
} // namespace Hyperion
