#include "Hyperion/IO/IOService.h"
#include "Hyperion/IO/Path.h"
#include <fstream>
#include <windows.h>

namespace Hyperion
{
namespace
{
void RequireLocalPath(const std::filesystem::path& InPath)
{
	if (IsPackagePath(InPath))
	{
		throw std::invalid_argument("Package path requires a configured mount: " + PathToUtf8(InPath));
	}
}
} // namespace

std::filesystem::path FLocalFileSystem::Normalize(const std::filesystem::path& InPath) const
{
	RequireLocalPath(InPath);
	return NormalizeFilePath(InPath);
}

bool FLocalFileSystem::Exists(const std::filesystem::path& InPath)
{
	RequireLocalPath(InPath);
	return std::filesystem::exists(InPath);
}

std::vector<std::filesystem::path> FLocalFileSystem::Enumerate(const std::filesystem::path& InDirectory,
                                                               bool bInRecursive)
{
	RequireLocalPath(InDirectory);
	std::vector<std::filesystem::path> Result;
	if (!std::filesystem::exists(InDirectory))
	{
		return Result;
	}
	const auto Append = [&](const std::filesystem::directory_entry& InEntry)
	{
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

FBytes FLocalFileSystem::Read(const std::filesystem::path& InPath, std::size_t InLimit)
{
	RequireLocalPath(InPath);
	std::ifstream File(InPath, std::ios::binary | std::ios::ate);
	if (!File)
	{
		std::error_code Error;
		if (!std::filesystem::exists(InPath, Error) && !Error)
		{
			throw FFileNotFound("File not found: " + InPath.generic_string());
		}
		throw std::runtime_error("Cannot read file: " + InPath.generic_string());
	}
	const auto Size = File.tellg();
	if (Size < 0 || static_cast<std::uint64_t>(Size) > InLimit)
	{
		throw std::runtime_error("Invalid file size or read limit exceeded");
	}
	FBytes Bytes(static_cast<std::size_t>(Size));
	File.seekg(0);
	if (!File.read(reinterpret_cast<char*>(Bytes.data()), Size))
	{
		throw std::runtime_error("Incomplete file read");
	}
	return Bytes;
}

void FLocalFileSystem::WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes)
{
	RequireLocalPath(InPath);
	static std::atomic_uint64_t Next{};
	if (!InPath.parent_path().empty())
	{
		std::filesystem::create_directories(InPath.parent_path());
	}
	auto Temporary = InPath;
	Temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(Next.fetch_add(1));
	try
	{
		{
			std::ofstream File(Temporary, std::ios::binary | std::ios::trunc);
			File.write(reinterpret_cast<const char*>(InBytes.data()), static_cast<std::streamsize>(InBytes.size()));
			File.flush();
			if (!File)
			{
				throw std::runtime_error("Cannot write temporary asset file");
			}
		}
		if (!MoveFileExW(Temporary.c_str(), InPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			throw std::runtime_error("Cannot replace asset file");
		}
	}
	catch (...)
	{
		std::error_code Error;
		std::filesystem::remove(Temporary, Error);
		throw;
	}
}
} // namespace Hyperion
