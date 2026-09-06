#include "Hyperion/D3D12/D3D12RHIBackend.h"
#include "D3D12RHIDevice.h"

namespace Hyperion
{
namespace
{
class FD3D12RHIBackend final : public IRHIBackend
{
public:
	ERHIBackend GetBackend() const noexcept override
	{
		return ERHIBackend::D3D12;
	}

	std::unique_ptr<IRHIDevice> CreateDevice(const FRHIDeviceDesc& InDesc) override
	{
		return std::make_unique<FD3D12RHIDevice>(InDesc);
	}
};
} // namespace

void RegisterD3D12RHIBackend(FRHIBackendRegistry& InRegistry)
{
	InRegistry.Register(std::make_unique<FD3D12RHIBackend>());
}
} // namespace Hyperion
