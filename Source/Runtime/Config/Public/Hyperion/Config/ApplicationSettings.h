#pragma once
#include "Hyperion/Config/AppSettings.h"
#include "Hyperion/Tasks/AsyncResult.h"

namespace Hyperion
{
struct FApplicationSettingsState
{
	FAppSettings Values;
	std::uint64_t Revision{};
	bool bActiveReversedZ{};
};

class IApplicationSettings
{
public:
	virtual ~IApplicationSettings() = default;
	virtual FApplicationSettingsState ApplicationSettings() const = 0;
	virtual void EditApplicationSettings(std::uint64_t InRevision, const FAppSettings& InSettings) = 0;
	virtual TAsyncResult<bool> SaveApplicationSettings(const std::filesystem::path& InPath) = 0;
	virtual void ChangeProfiling(std::optional<std::uint32_t> InMask, std::optional<bool> InSampling) = 0;
};

template<> const FRecordDescriptor& RecordType<FApplicationSettingsState>();
} // namespace Hyperion
