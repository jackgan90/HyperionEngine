#pragma once
#include "Hyperion/RHI/RHIDevice.h"
#include <map>

namespace Hyperion
{
class IRHIBackend
{
public:
	virtual ~IRHIBackend() = default;
	virtual ERHIBackend GetBackend() const noexcept = 0;
	virtual std::unique_ptr<IRHIDevice> CreateDevice(const FRHIDeviceDesc& InDesc) = 0;
};

// Register compiled providers at application startup, then create devices on RHI 0.
// No global registration or concrete native backend dependency exists in RHI.
class FRHIBackendRegistry
{
public:
	void Register(std::unique_ptr<IRHIBackend> InBackend);
	bool IsRegistered(ERHIBackend InBackend) const;
	std::unique_ptr<IRHIDevice> CreateDevice(ERHIBackend InBackend, const FRHIDeviceDesc& InDesc = {}) const;

private:
	std::map<ERHIBackend, std::unique_ptr<IRHIBackend>> Backends;
};
} // namespace Hyperion
