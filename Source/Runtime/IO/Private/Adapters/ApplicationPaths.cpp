#include "Hyperion/IO/ApplicationPaths.h"
#include "Hyperion/IO/Path.h"
#include <shlobj.h>
#include <stdexcept>
#include <vector>
#include <windows.h>

namespace Hyperion
{
std::string EnvironmentValue(std::string_view InName)
{
	const auto Name = PathFromUtf8(InName).native();
	const auto Size = GetEnvironmentVariableW(Name.c_str(), nullptr, 0);
	if (!Size)
	{
		return {};
	}
	std::wstring Value(Size, L'\0');
	const auto Written = GetEnvironmentVariableW(Name.c_str(), Value.data(), Size);
	if (!Written || Written >= Size)
	{
		throw std::runtime_error("Cannot read environment override: " + std::string(InName));
	}
	Value.resize(Written);
	return PathToUtf8(std::filesystem::path(Value));
}

std::filesystem::path LocalUserDirectory()
{
	PWSTR Path{};
	if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DONT_VERIFY, nullptr, &Path)))
	{
		throw std::runtime_error("Cannot locate local user directory; specify --storage-settings and storage roots");
	}
	try
	{
		std::filesystem::path Result(Path);
		CoTaskMemFree(Path);
		return Result;
	}
	catch (...)
	{
		CoTaskMemFree(Path);
		throw;
	}
}

std::filesystem::path ExecutableDirectory()
{
	std::vector<wchar_t> Buffer(32768);
	const auto Size = GetModuleFileNameW(nullptr, Buffer.data(), static_cast<DWORD>(Buffer.size()));
	if (!Size || Size >= Buffer.size())
	{
		throw std::runtime_error("Cannot locate application executable");
	}
	return std::filesystem::path(std::wstring(Buffer.data(), Size)).parent_path();
}

void RequireUnlinkedPath(const std::filesystem::path& InPath)
{
	std::filesystem::path Current;
	for (const auto& Part : NormalizeFilePath(InPath))
	{
		Current /= Part;
		const auto Attributes = GetFileAttributesW(Current.c_str());
		if (Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_REPARSE_POINT))
		{
			throw std::invalid_argument("Storage path crosses a link/junction: " + PathToUtf8(Current));
		}
	}
}
} // namespace Hyperion
