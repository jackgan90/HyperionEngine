#include "Hyperion/Automation/Targets.h"
#include <algorithm>
#include <fstream>

namespace Hyperion
{
namespace
{
void ValidateInstance(const std::string& InInstance)
{
	if (InInstance.size() != 32 || InInstance.find_first_not_of("0123456789abcdef") != std::string::npos)
	{
		throw FAutomationError("invalid_arguments", "Invalid target instance identity");
	}
}
} // namespace

FLocalTargetDiscovery::FLocalTargetDiscovery(std::filesystem::path InDirectory) : Directory(std::move(InDirectory))
{
}

std::vector<FAutomationTarget> FLocalTargetDiscovery::List()
{
	if (Directory.empty())
	{
		Directory = LocalDiscoveryDirectory();
	}
	std::vector<FAutomationTarget> Targets;
	if (!std::filesystem::exists(Directory))
	{
		return Targets;
	}
	std::size_t Examined{};
	for (const auto& Entry : std::filesystem::directory_iterator(Directory))
	{
		if (++Examined > 1024 || Targets.size() >= 128)
		{
			break;
		}
		try
		{
			if (Entry.is_symlink() || !Entry.is_regular_file() || Entry.path().extension() != ".json" ||
			    Entry.file_size() > 8192)
			{
				continue;
			}
			std::ifstream File(Entry.path(), std::ios::binary);
			std::string Text(8193, '\0');
			File.read(Text.data(), static_cast<std::streamsize>(Text.size()));
			Text.resize(static_cast<std::size_t>(File.gcount()));
			const auto Value = ReadRecordWire(RecordType<FAutomationTarget>(), ParseJson(Text, {8192, 128, 8}));
			auto Target = *static_cast<const FAutomationTarget*>(Value.get());
			ValidateInstance(Target.Instance);
			if (Entry.path().stem().string() == Target.Instance)
			{
				Targets.push_back(std::move(Target));
			}
		}
		catch (const std::exception&)
		{
			// A stale, partial or foreign record never becomes authoritative connection state.
		}
	}
	std::sort(Targets.begin(), Targets.end(),
	          [](const auto& InLeft, const auto& InRight)
	          {
		          return InLeft.Instance < InRight.Instance;
	          });
	return Targets;
}

void FLocalTargetDiscovery::Publish(const FAutomationTarget& InTarget)
{
	if (Directory.empty())
	{
		Directory = LocalDiscoveryDirectory();
	}
	ValidateInstance(InTarget.Instance);
	std::filesystem::create_directories(Directory);
	const auto Temporary = Directory / (InTarget.Instance + ".tmp");
	{
		std::ofstream File(Temporary, std::ios::binary | std::ios::trunc);
		File << WriteJson(WriteRecordWire(RecordType<FAutomationTarget>(), &InTarget), {8192, 128, 8});
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
		ValidateInstance(InInstance);
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
