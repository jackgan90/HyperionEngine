#include "TargetRegistration.h"
#include <algorithm>
#include <fstream>

namespace Hyperion
{
namespace Private
{
inline constexpr std::uint32_t LocalTargetRecordVersion = 1;

struct FLocalTargetEnvelope
{
	std::uint32_t RecordVersion = LocalTargetRecordVersion;
	FAutomationTarget Target;
	FTargetProcessIdentity Owner;
};
} // namespace Private

template<> const FRecordDescriptor& RecordType<Private::FTargetProcessIdentity>()
{
	using FIdentity = Private::FTargetProcessIdentity;
	static const auto Type = MakeRecord<FIdentity>(
	    "automation.local.owner", {Member("processId", &FIdentity::ProcessId, {.bRequired = true}),
	                               Member("creationTime", &FIdentity::CreationTime, {.bRequired = true})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<Private::FLocalTargetEnvelope>()
{
	using FEnvelope = Private::FLocalTargetEnvelope;
	static const auto Type = MakeRecord<FEnvelope>(
	    "automation.local.registration", {Member("recordVersion", &FEnvelope::RecordVersion, {.bRequired = true}),
	                                      Member("target", &FEnvelope::Target, {.bRequired = true}),
	                                      Member("owner", &FEnvelope::Owner, {.bRequired = true})});
	return Type;
}

namespace Private
{
void ValidateTargetInstance(const std::string& InInstance)
{
	if (InInstance.size() != 32 || InInstance.find_first_not_of("0123456789abcdef") != std::string::npos)
	{
		throw FAutomationError("invalid_arguments", "Invalid target instance identity");
	}
}

std::optional<FTargetRegistration> ReadTargetRegistration(const std::filesystem::path& InPath)
{
	try
	{
		if (InPath.extension() != ".json" ||
		    !std::filesystem::is_regular_file(std::filesystem::symlink_status(InPath)) ||
		    std::filesystem::file_size(InPath) > MaxTargetRecordBytes)
		{
			return {};
		}
		std::ifstream File(InPath, std::ios::binary);
		FTargetRegistration Record;
		Record.Text.resize(MaxTargetRecordBytes + 1);
		File.read(Record.Text.data(), static_cast<std::streamsize>(Record.Text.size()));
		Record.Text.resize(static_cast<std::size_t>(File.gcount()));
		const auto Json = ParseJson(Record.Text, {MaxTargetRecordBytes, 128, 8});
		const auto& Fields = std::get<FArchiveNode::FObject>(Json.Value);
		if (Fields.contains("recordVersion"))
		{
			const auto Value = ReadRecordWire(RecordType<FLocalTargetEnvelope>(), Json);
			const auto& Envelope = *static_cast<const FLocalTargetEnvelope*>(Value.get());
			if (Envelope.RecordVersion != LocalTargetRecordVersion || !Envelope.Owner.ProcessId ||
			    !Envelope.Owner.CreationTime)
			{
				return {};
			}
			Record.Target = Envelope.Target;
			Record.Owner = Envelope.Owner;
		}
		else
		{
			const auto Value = ReadRecordWire(RecordType<FAutomationTarget>(), Json);
			Record.Target = *static_cast<const FAutomationTarget*>(Value.get());
		}
		ValidateTargetInstance(Record.Target.Instance);
		if (InPath.stem().string() == Record.Target.Instance)
		{
			return Record;
		}
	}
	catch (const std::exception&)
	{
		// Partial, unsupported or foreign records cannot establish a candidate or permit deletion.
	}
	return {};
}

std::string WriteTargetRegistration(const FAutomationTarget& InTarget, const FTargetProcessIdentity& InOwner)
{
	const FLocalTargetEnvelope Envelope{LocalTargetRecordVersion, InTarget, InOwner};
	return WriteJson(WriteRecordWire(RecordType<FLocalTargetEnvelope>(), &Envelope), {MaxTargetRecordBytes, 128, 8});
}

ETargetProcessState RegistrationState(const FTargetRegistration& InRecord, FTargetProcessQuery InQuery)
{
	return InRecord.Owner ? InQuery(*InRecord.Owner) : ETargetProcessState::Unknown;
}

FTargetDiscoverySnapshot CollectLocalTargets(const std::filesystem::path& InDirectory, FTargetProcessQuery InQuery)
{
	FTargetDiscoverySnapshot Result;
	if (!std::filesystem::exists(InDirectory))
	{
		return Result;
	}
	for (const auto& Entry : std::filesystem::directory_iterator(InDirectory))
	{
		if (Result.Examined >= MaxTargetScanEntries || Result.Targets.size() >= MaxTargetResults)
		{
			Result.StopReason = Result.Examined >= MaxTargetScanEntries ? ETargetListStopReason::ScanLimit
			                                                            : ETargetListStopReason::ResultLimit;
			break;
		}
		++Result.Examined;
		const auto Record = ReadTargetRegistration(Entry.path());
		if (!Record)
		{
			continue;
		}
		const auto State = RegistrationState(*Record, InQuery);
		if (State == ETargetProcessState::Dead)
		{
			++Result.StaleSkipped;
			continue;
		}
		Result.UnknownOwnership += State == ETargetProcessState::Unknown ? 1 : 0;
		Result.Targets.push_back(Record->Target);
	}
	std::sort(Result.Targets.begin(), Result.Targets.end(),
	          [](const auto& InLeft, const auto& InRight)
	          {
		          return InLeft.Instance < InRight.Instance;
	          });
	return Result;
}

void PruneLocalTargets(const std::filesystem::path& InDirectory, FTargetProcessQuery InQuery) noexcept
{
	try
	{
		std::uint32_t Examined{};
		for (const auto& Entry : std::filesystem::directory_iterator(InDirectory))
		{
			if (Examined++ >= MaxTargetScanEntries)
			{
				break;
			}
			const auto Record = ReadTargetRegistration(Entry.path());
			if (Record && RegistrationState(*Record, InQuery) == ETargetProcessState::Dead)
			{
				RemoveUnchangedTargetRecord(Entry.path(), Record->Text);
			}
		}
	}
	catch (const std::exception&)
	{
		// Best-effort maintenance is never a prerequisite for a healthy target to publish.
	}
}
} // namespace Private
} // namespace Hyperion
