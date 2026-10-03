#include "../TargetRegistration.h"
#include <Windows.h>

namespace Hyperion::Private
{
namespace
{
class FRegistrationHandle
{
public:
	explicit FRegistrationHandle(HANDLE InValue) : Value(InValue)
	{
	}

	~FRegistrationHandle()
	{
		if (Value && Value != INVALID_HANDLE_VALUE)
		{
			CloseHandle(Value);
		}
	}

	FRegistrationHandle(const FRegistrationHandle&) = delete;
	FRegistrationHandle& operator=(const FRegistrationHandle&) = delete;
	HANDLE Value;
};

std::optional<std::uint64_t> ProcessCreationTime(HANDLE InProcess)
{
	FILETIME Created{};
	FILETIME Exited{};
	FILETIME Kernel{};
	FILETIME User{};
	if (!GetProcessTimes(InProcess, &Created, &Exited, &Kernel, &User))
	{
		return {};
	}
	return (static_cast<std::uint64_t>(Created.dwHighDateTime) << 32) | Created.dwLowDateTime;
}
} // namespace

FTargetProcessIdentity CurrentTargetProcess()
{
	const auto Created = ProcessCreationTime(GetCurrentProcess());
	if (!Created || !*Created)
	{
		throw FAutomationError("discovery_unavailable", "Could not identify local registration owner");
	}
	return {GetCurrentProcessId(), *Created};
}

ETargetProcessState QueryTargetProcess(const FTargetProcessIdentity& InOwner)
{
	if (!InOwner.ProcessId || !InOwner.CreationTime)
	{
		return ETargetProcessState::Unknown;
	}
	const FRegistrationHandle Process(
	    OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, InOwner.ProcessId));
	if (!Process.Value)
	{
		return GetLastError() == ERROR_INVALID_PARAMETER ? ETargetProcessState::Dead : ETargetProcessState::Unknown;
	}
	const auto Created = ProcessCreationTime(Process.Value);
	if (!Created)
	{
		return ETargetProcessState::Unknown;
	}
	if (*Created != InOwner.CreationTime)
	{
		return ETargetProcessState::Dead;
	}
	switch (WaitForSingleObject(Process.Value, 0))
	{
		case WAIT_OBJECT_0:
			return ETargetProcessState::Dead;
		case WAIT_TIMEOUT:
			return ETargetProcessState::Alive;
		default:
			return ETargetProcessState::Unknown;
	}
}

bool RemoveUnchangedTargetRecord(const std::filesystem::path& InPath, const std::string& InExpectedText)
{
	if (InExpectedText.size() > MaxTargetRecordBytes)
	{
		return false;
	}
	// Excluding write and delete sharing pins this file and its bytes through disposition.
	const FRegistrationHandle File(CreateFileW(InPath.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ, nullptr,
	                                           OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
	if (File.Value == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	BY_HANDLE_FILE_INFORMATION Info{};
	if (!GetFileInformationByHandle(File.Value, &Info) ||
	    (Info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || Info.nFileSizeHigh ||
	    Info.nFileSizeLow != InExpectedText.size())
	{
		return false;
	}
	std::string Text(InExpectedText.size(), '\0');
	DWORD Read{};
	if (!ReadFile(File.Value, Text.data(), static_cast<DWORD>(Text.size()), &Read, nullptr) || Read != Text.size() ||
	    Text != InExpectedText)
	{
		return false;
	}
	FILE_DISPOSITION_INFO Disposition{TRUE};
	return SetFileInformationByHandle(File.Value, FileDispositionInfo, &Disposition, sizeof(Disposition)) != FALSE;
}
} // namespace Hyperion::Private
