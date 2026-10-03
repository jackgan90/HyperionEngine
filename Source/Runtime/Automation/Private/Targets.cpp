#include "Hyperion/Automation/Targets.h"
#include "TargetRegistration.h"
#include <fstream>

namespace Hyperion
{
FLocalTargetDiscovery::FLocalTargetDiscovery(std::filesystem::path InDirectory) : Directory(std::move(InDirectory))
{
}

FTargetDiscoverySnapshot FLocalTargetDiscovery::List()
{
	if (Directory.empty())
	{
		Directory = LocalDiscoveryDirectory();
	}
	return Private::CollectLocalTargets(Directory);
}

std::optional<FAutomationTarget> FLocalTargetDiscovery::Find(const std::string& InInstance)
{
	Private::ValidateTargetInstance(InInstance);
	if (Directory.empty())
	{
		Directory = LocalDiscoveryDirectory();
	}
	const auto Record = Private::ReadTargetRegistration(Directory / (InInstance + ".json"));
	if (Record &&
	    Private::RegistrationState(*Record, Private::QueryTargetProcess) != Private::ETargetProcessState::Dead)
	{
		return Record->Target;
	}
	return {};
}

void FLocalTargetDiscovery::Publish(const FAutomationTarget& InTarget)
{
	if (Directory.empty())
	{
		Directory = LocalDiscoveryDirectory();
	}
	Private::ValidateTargetInstance(InTarget.Instance);
	std::filesystem::create_directories(Directory);
	Private::PruneLocalTargets(Directory);
	const auto Temporary = Directory / (InTarget.Instance + ".tmp");
	{
		std::ofstream File(Temporary, std::ios::binary | std::ios::trunc);
		File << Private::WriteTargetRegistration(InTarget, Private::CurrentTargetProcess());
		File.flush();
		if (!File)
		{
			throw FAutomationError("discovery_unavailable", "Could not publish local target record");
		}
	}
	std::filesystem::rename(Temporary, Directory / (InTarget.Instance + ".json"));
}

void FLocalTargetDiscovery::Withdraw(const std::string& InInstance) noexcept
{
	if (Directory.empty())
	{
		return;
	}
	try
	{
		Private::ValidateTargetInstance(InInstance);
		std::error_code Error;
		std::filesystem::remove(Directory / (InInstance + ".json"), Error);
	}
	catch (...)
	{
	}
}

bool FCurrentUserAccessPolicy::Admit(const FTransportPeer& InPeer) const
{
	return InPeer.bAuthenticated && InPeer.bLocal && InPeer.bCurrentUser;
}
} // namespace Hyperion
