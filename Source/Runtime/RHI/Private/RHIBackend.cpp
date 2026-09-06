#include "Hyperion/RHI/RHIBackend.h"
#include <stdexcept>

namespace Hyperion
{
std::string_view GetRHIBackendName(ERHIBackend InBackend)
{
	switch (InBackend)
	{
		case ERHIBackend::D3D12:
			return "d3d12";
		case ERHIBackend::Vulkan:
			return "vulkan";
		case ERHIBackend::Metal:
			return "metal";
		default:
			throw std::invalid_argument("Unknown RHI backend enum");
	}
}

ERHIBackend ParseRHIBackend(std::string_view InName)
{
	for (ERHIBackend Backend : {ERHIBackend::D3D12, ERHIBackend::Vulkan, ERHIBackend::Metal})
	{
		if (GetRHIBackendName(Backend) == InName)
		{
			return Backend;
		}
	}
	throw std::invalid_argument("Unknown RHI backend: " + std::string(InName));
}

FRHIFeatureSupport FRHICapabilities::QueryFeature(ERHIFeature InFeature) const
{
	return Features.at(static_cast<std::size_t>(InFeature));
}

void ValidateRequiredFeatures(const FRHIDeviceDesc& InDesc, const FRHICapabilities& InCapabilities)
{
	for (ERHIFeature Feature : InDesc.RequiredFeatures)
	{
		const FRHIFeatureSupport Support = InCapabilities.QueryFeature(Feature);
		if (!Support.Supported || !Support.Enabled)
		{
			throw std::runtime_error("Required RHI feature cannot be enabled by " +
			                         std::string(GetRHIBackendName(InCapabilities.Backend)) + ": " +
			                         std::to_string(static_cast<int>(Feature)));
		}
	}
	for (ERHIFeature Feature : InDesc.OptionalFeatures)
	{
		(void)InCapabilities.QueryFeature(Feature); // Reject malformed enums even for optional requests.
	}
}

void FRHIBackendRegistry::Register(std::unique_ptr<IRHIBackend> InBackend)
{
	if (!InBackend)
	{
		throw std::invalid_argument("Null RHI backend provider");
	}
	const ERHIBackend Backend = InBackend->GetBackend();
	(void)GetRHIBackendName(Backend);
	if (Backends.contains(Backend))
	{
		throw std::invalid_argument("RHI backend already registered");
	}
	Backends.emplace(Backend, std::move(InBackend));
}

bool FRHIBackendRegistry::IsRegistered(ERHIBackend InBackend) const
{
	return Backends.contains(InBackend);
}

std::unique_ptr<IRHIDevice> FRHIBackendRegistry::CreateDevice(ERHIBackend InBackend, const FRHIDeviceDesc& InDesc) const
{
	const auto Provider = Backends.find(InBackend);
	if (Provider == Backends.end())
	{
		throw std::runtime_error("RHI backend is not registered: " + std::string(GetRHIBackendName(InBackend)));
	}
	std::unique_ptr<IRHIDevice> Device = Provider->second->CreateDevice(InDesc);
	if (!Device || Device->GetCapabilities().Backend != InBackend)
	{
		throw std::runtime_error("RHI provider returned a null or mismatched device");
	}
	ValidateRequiredFeatures(InDesc, Device->GetCapabilities());
	return Device;
}
} // namespace Hyperion
