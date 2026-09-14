#include "../LocalPath.h"
#include "Hyperion/IO/Path.h"
#include <windows.h>

namespace Hyperion
{
std::vector<std::filesystem::path> EnumerateLocalContent(const std::filesystem::path& InDirectory, bool bInRecursive)
{
	std::vector<std::filesystem::path> Result;
	if (!std::filesystem::exists(InDirectory))
	{
		return Result;
	}
	const auto Append = [&](const std::filesystem::directory_entry& InEntry)
	{
		const auto Attributes = GetFileAttributesW(InEntry.path().c_str());
		if (Attributes == INVALID_FILE_ATTRIBUTES || (Attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			throw std::runtime_error("Cannot enumerate linked or inaccessible content: " + PathToUtf8(InEntry.path()));
		}
		if (InEntry.is_regular_file())
		{
			Result.push_back(InEntry.path());
		}
	};
	if (bInRecursive)
	{
		for (const auto& Entry : std::filesystem::recursive_directory_iterator(InDirectory))
		{
			Append(Entry);
		}
	}
	else
	{
		for (const auto& Entry : std::filesystem::directory_iterator(InDirectory))
		{
			Append(Entry);
		}
	}
	return Result;
}

void ValidateLocalPathCase(const std::filesystem::path& InRoot, const std::filesystem::path& InRelative)
{
	auto Path = InRoot;
	for (const auto& Part : InRelative)
	{
		if (Part == "." || Part.empty())
		{
			continue;
		}
		Path /= Part;
		WIN32_FIND_DATAW Entry{};
		const auto Handle = FindFirstFileW(Path.c_str(), &Entry);
		if (Handle == INVALID_HANDLE_VALUE)
		{
			const auto Error = GetLastError();
			if (Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND)
			{
				return;
			}
			throw std::runtime_error("Cannot inspect package path: " + PathToUtf8(Path));
		}
		FindClose(Handle);
		if ((Entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			throw std::runtime_error("Package path traverses a filesystem link: " + PathToUtf8(Path));
		}
		if (Part.native() != Entry.cFileName)
		{
			throw std::runtime_error("Package path case mismatch: " + PathToUtf8(Path));
		}
	}
}
} // namespace Hyperion
