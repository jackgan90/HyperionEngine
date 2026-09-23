#include "Hyperion/Transport/Transport.h"

namespace Hyperion
{
FTransportError::FTransportError(std::string InCode, std::string InMessage)
    : std::runtime_error(std::move(InMessage)), Code(std::move(InCode))
{
}

const std::string& FTransportError::GetCode() const noexcept
{
	return Code;
}

void FTransportRegistry::Register(std::unique_ptr<ITransportProvider> InProvider)
{
	if (!InProvider || InProvider->Scheme().empty())
	{
		throw std::invalid_argument("A transport provider requires a scheme");
	}
	const auto Scheme = InProvider->Scheme();
	if (Providers.contains(Scheme))
	{
		throw std::invalid_argument("Duplicate transport scheme: " + Scheme);
	}
	Providers.emplace(Scheme, std::move(InProvider));
}

ITransportProvider& FTransportRegistry::Find(const std::string& InScheme) const
{
	const auto Found = Providers.find(InScheme);
	if (Found == Providers.end())
	{
		throw FTransportError("unsupported_transport", "Transport provider is unavailable: " + InScheme);
	}
	return *Found->second;
}

std::unique_ptr<ITransportConnection> FTransportRegistry::Connect(const FTransportAddress& InAddress) const
{
	return Find(InAddress.Scheme).Connect(InAddress.Address);
}

std::unique_ptr<ITransportListener> FTransportRegistry::Listen(const FTransportAddress& InAddress) const
{
	return Find(InAddress.Scheme).Listen(InAddress.Address);
}
} // namespace Hyperion
