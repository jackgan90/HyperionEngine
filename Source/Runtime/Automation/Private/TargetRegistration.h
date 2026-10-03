#pragma once
#include "Hyperion/Automation/Targets.h"

namespace Hyperion::Private
{
inline constexpr std::size_t MaxTargetRecordBytes = 8192;
inline constexpr std::uint32_t MaxTargetScanEntries = 1024;
inline constexpr std::size_t MaxTargetResults = 128;

struct FTargetProcessIdentity
{
	std::uint32_t ProcessId{};
	std::uint64_t CreationTime{};
};

enum class ETargetProcessState
{
	Alive,
	Dead,
	Unknown
};

struct FTargetRegistration
{
	FAutomationTarget Target;
	std::optional<FTargetProcessIdentity> Owner;
	std::string Text;
};

using FTargetProcessQuery = ETargetProcessState (*)(const FTargetProcessIdentity&);
FTargetProcessIdentity CurrentTargetProcess();
ETargetProcessState QueryTargetProcess(const FTargetProcessIdentity& InOwner);
bool RemoveUnchangedTargetRecord(const std::filesystem::path& InPath, const std::string& InExpectedText);

void ValidateTargetInstance(const std::string& InInstance);
std::optional<FTargetRegistration> ReadTargetRegistration(const std::filesystem::path& InPath);
std::string WriteTargetRegistration(const FAutomationTarget& InTarget, const FTargetProcessIdentity& InOwner);
ETargetProcessState RegistrationState(const FTargetRegistration& InRecord, FTargetProcessQuery InQuery);
FTargetDiscoverySnapshot CollectLocalTargets(const std::filesystem::path& InDirectory,
                                             FTargetProcessQuery InQuery = QueryTargetProcess);
void PruneLocalTargets(const std::filesystem::path& InDirectory,
                       FTargetProcessQuery InQuery = QueryTargetProcess) noexcept;
} // namespace Hyperion::Private
