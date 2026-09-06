#include "Hyperion/IO/IOService.h"
#include <fstream>
#include <windows.h>

namespace Hyperion
{
FBytes FLocalFileSystem::Read(const std::filesystem::path& InPath, std::size_t InLimit)
{
	std::ifstream File(InPath, std::ios::binary | std::ios::ate);
	if (!File)
	{
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
