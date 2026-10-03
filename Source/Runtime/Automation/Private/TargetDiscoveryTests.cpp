#include "Hyperion/Automation/Connections.h"
#include "TargetRegistration.h"
#include <fstream>
#include <source_location>

namespace Hyperion
{
namespace
{
using namespace Private;

void Check(bool bInCondition, std::source_location InLocation = std::source_location::current())
{
	if (!bInCondition)
	{
		throw std::runtime_error("Discovery check failed: " + std::to_string(InLocation.line()));
	}
}

class FDiscoveryFixture
{
public:
	FDiscoveryFixture()
	    : Directory(std::filesystem::temp_directory_path() / ("HyperionDiscovery-" + CreateAutomationIdentity()))
	{
		std::filesystem::create_directories(Directory);
	}

	~FDiscoveryFixture()
	{
		std::error_code Error;
		std::filesystem::remove_all(Directory, Error);
	}

	std::filesystem::path Write(const FAutomationTarget& InTarget, const std::string& InText)
	{
		const auto Path = Directory / (InTarget.Instance + ".json");
		std::ofstream(Path, std::ios::binary) << InText;
		return Path;
	}

	std::filesystem::path Directory;
};

FAutomationTarget NewTarget()
{
	return {CreateAutomationIdentity(), "DiscoveryTest", "build", {"memory", "discovery"}, "Test", "fixture"};
}

std::string Legacy(const FAutomationTarget& InTarget)
{
	return WriteJson(WriteRecordWire(RecordType<FAutomationTarget>(), &InTarget));
}

ETargetProcessState Unknown(const FTargetProcessIdentity&)
{
	return ETargetProcessState::Unknown;
}

ETargetProcessState Dead(const FTargetProcessIdentity&)
{
	return ETargetProcessState::Dead;
}

void Ownership()
{
	FDiscoveryFixture Fixture;
	const auto Owner = CurrentTargetProcess();
	Check(QueryTargetProcess(Owner) == ETargetProcessState::Alive);
	Check(QueryTargetProcess({}) == ETargetProcessState::Unknown);
	auto Reused = Owner;
	++Reused.CreationTime;
	Check(QueryTargetProcess(Reused) == ETargetProcessState::Dead);
	const auto Live = NewTarget();
	const auto Old = NewTarget();
	const auto Stale = NewTarget();
	const auto LivePath = Fixture.Write(Live, WriteTargetRegistration(Live, Owner));
	const auto OldPath = Fixture.Write(Old, Legacy(Old));
	const auto StalePath = Fixture.Write(Stale, WriteTargetRegistration(Stale, Reused));
	FLocalTargetDiscovery Discovery(Fixture.Directory);
	Check(Discovery.Find(Live.Instance) && Discovery.Find(Old.Instance) && !Discovery.Find(Stale.Instance));
	const auto Listed = Discovery.List();
	Check(Listed.Targets.size() == 2 && Listed.Examined == 3 && Listed.StaleSkipped == 1 &&
	      Listed.UnknownOwnership == 1);
	Check(Listed.StopReason == ETargetListStopReason::Complete && std::filesystem::exists(StalePath));
	const auto Uncertain = CollectLocalTargets(Fixture.Directory, Unknown);
	Check(Uncertain.Targets.size() == 3 && Uncertain.UnknownOwnership == 3 && !Uncertain.StaleSkipped);
	PruneLocalTargets(Fixture.Directory, Unknown);
	Check(std::filesystem::exists(StalePath));
	Discovery.Publish(NewTarget());
	Check(!std::filesystem::exists(StalePath) && std::filesystem::exists(LivePath) && std::filesystem::exists(OldPath));
	Discovery.Withdraw(Live.Instance);
	Check(!Discovery.Find(Live.Instance));
}

void SafeRemoval()
{
	FDiscoveryFixture Fixture;
	const auto Target = NewTarget();
	const auto Text = WriteTargetRegistration(Target, CurrentTargetProcess());
	const auto Path = Fixture.Write(Target, Text);
	Check(!RemoveUnchangedTargetRecord(Path, Text + " ") && std::filesystem::exists(Path));
	// A writer keeps the record intact; cleanup must not race a publication in progress.
	{
		std::ofstream Writer(Path, std::ios::binary | std::ios::app);
		Check(!RemoveUnchangedTargetRecord(Path, Text) && std::filesystem::exists(Path));
	}
	Fixture.Write(Target, Text + " ");
	Check(!RemoveUnchangedTargetRecord(Path, Text));
	Check(RemoveUnchangedTargetRecord(Path, Text + " ") && !std::filesystem::exists(Path));
	const auto LegacyPath = Fixture.Write(Target, Legacy(Target));
	PruneLocalTargets(Fixture.Directory, Dead);
	Check(std::filesystem::exists(LegacyPath));
	const auto Linked = NewTarget();
	const auto LinkPath = Fixture.Directory / (Linked.Instance + ".json");
	std::error_code Error;
	std::filesystem::create_symlink(LegacyPath, LinkPath, Error);
	if (!Error)
	{
		Check(!ReadTargetRegistration(LinkPath));
		Check(!RemoveUnchangedTargetRecord(LinkPath, Legacy(Target)));
		Check(std::filesystem::exists(LegacyPath));
	}
}

void InvalidRecords()
{
	FDiscoveryFixture Fixture;
	FLocalTargetDiscovery Discovery(Fixture.Directory);
	const auto Target = NewTarget();
	auto Unsupported = ParseJson(WriteTargetRegistration(Target, CurrentTargetProcess()));
	std::get<FArchiveNode::FObject>(Unsupported.Value).at("recordVersion") = WriteValue(std::uint32_t{2});
	for (const auto& Text : {std::string("{"), std::string(MaxTargetRecordBytes + 1, ' '), Legacy(NewTarget()),
	                         WriteJson(Unsupported), WriteTargetRegistration(Target, {})})
	{
		const auto Path = Fixture.Write(Target, Text);
		Check(!Discovery.Find(Target.Instance));
		PruneLocalTargets(Fixture.Directory, Dead);
		Check(std::filesystem::exists(Path));
	}
	for (const auto& Instance : {std::string("../bad"), std::string(32, 'F'), std::string(33, 'a'), std::string()})
	{
		bool bRejected{};
		try
		{
			Discovery.Find(Instance);
		}
		catch (const FAutomationError&)
		{
			bRejected = true;
		}
		Check(bRejected);
	}
	Check(!Discovery.Find(CreateAutomationIdentity()));
}

void ListBudgets()
{
	FDiscoveryFixture Fixture;
	FLocalTargetDiscovery Discovery(Fixture.Directory);
	Check(Discovery.List().StopReason == ETargetListStopReason::Complete);
	for (std::size_t Index = 0; Index < MaxTargetResults; ++Index)
	{
		const auto Target = NewTarget();
		Fixture.Write(Target, Legacy(Target));
	}
	const auto Full = Discovery.List();
	Check(Full.StopReason == ETargetListStopReason::Complete && Full.Examined == MaxTargetResults);
	const auto Extra = NewTarget();
	Fixture.Write(Extra, Legacy(Extra));
	const auto Limited = Discovery.List();
	Check(Limited.StopReason == ETargetListStopReason::ResultLimit && Limited.Targets.size() == MaxTargetResults);
	Check(Limited.UnknownOwnership == MaxTargetResults && Limited.Examined == MaxTargetResults);
	FTransportRegistry Transports;
	FCurrentUserAccessPolicy Access;
	FConnectionManager Manager(Transports, Discovery, Access);
	const auto Response = Manager.List();
	const auto& Result =
	    std::get<FArchiveNode::FObject>(std::get<FArchiveNode::FObject>(Response.Value).at("result").Value);
	Check(ReadValue<bool>(Result.at("truncated")));
	Check(ReadValue<ETargetListStopReason>(Result.at("stopReason")) == ETargetListStopReason::ResultLimit);
}

void ScanBudget()
{
	FDiscoveryFixture Fixture;
	for (std::uint32_t Index = 0; Index < MaxTargetScanEntries; ++Index)
	{
		std::ofstream(Fixture.Directory / (std::to_string(Index) + ".tmp")) << "foreign";
	}
	FLocalTargetDiscovery Discovery(Fixture.Directory);
	Check(Discovery.List().StopReason == ETargetListStopReason::Complete);
	const auto Target = NewTarget();
	Fixture.Write(Target, Legacy(Target));
	const auto Snapshot = Discovery.List();
	Check(Snapshot.StopReason == ETargetListStopReason::ScanLimit && Snapshot.Examined == MaxTargetScanEntries);
	Check(Discovery.Find(Target.Instance)->Instance == Target.Instance);
}
} // namespace

void RunDiscoveryTests()
{
	Ownership();
	SafeRemoval();
	InvalidRecords();
	ListBudgets();
	ScanBudget();
}
} // namespace Hyperion
