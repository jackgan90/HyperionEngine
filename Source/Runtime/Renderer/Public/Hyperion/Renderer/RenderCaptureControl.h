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

struct FRenderCaptureHudInfo
{
	std::optional<bool> Preference;
	std::optional<bool> Enabled;
};

class IRenderCaptureControl
{
public:
	virtual ~IRenderCaptureControl() = default;
	virtual FRenderCaptureInfo RenderCaptureInfo() const = 0;
	virtual void RequestRenderCapture() = 0;
	virtual void OpenRenderCapture() = 0;
	virtual void SetRenderCapturePreference(bool bInEnabled);
	virtual FRenderCaptureHudInfo RenderCaptureHudInfo() const;
	virtual void SetRenderCaptureHudPreference(bool bInEnabled);
};

template<> const FRecordDescriptor& RecordType<FRenderCaptureInfo>();
template<> const FRecordDescriptor& RecordType<FRenderCaptureHudInfo>();
} // namespace Hyperion
