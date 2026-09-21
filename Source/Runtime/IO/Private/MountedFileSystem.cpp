#include "Hyperion/IO/MountedFileSystem.h"
#include "LocalPath.h"
#include <algorithm>
#include <cctype>

namespace Hyperion
{
namespace
{
std::string Fold(std::string InText)
{
	std::transform(InText.begin(), InText.end(), InText.begin(),
	               [](unsigned char InValue)
	               {
		               return static_cast<char>(std::tolower(InValue));
	               });
	return InText;
}

bool Within(const std::filesystem::path& InPath, const std::filesystem::path& InRoot)
{
	const auto Path = Fold(PathToUtf8(InPath));
	const auto Root = Fold(PathToUtf8(InRoot));
	return Path == Root || Path.starts_with(Root.ends_with('/') ? Root : Root + "/");
}

} // namespace

FMountedFileSystem::FMountedFileSystem(std::vector<FContentMount> InMounts) : Mounts(std::move(InMounts))
{
	for (auto& Mount : Mounts)
	{
		Mount.Root = NormalizeFilePath(Mount.Root);
		if (!IsPackagePath(Mount.Root) || Mount.Root == "/" || Mount.Root.parent_path() != "/")
		{
			throw std::invalid_argument("Mount must be a single package root");
		}
		Mount.Directory = std::filesystem::weakly_canonical(std::filesystem::absolute(Mount.Directory));
	}
	for (std::size_t Index = 0; Index < Mounts.size(); ++Index)
	{
		for (std::size_t Other = 0; Other < Index; ++Other)
		{
			if (Fold(PathToUtf8(Mounts[Index].Root)) == Fold(PathToUtf8(Mounts[Other].Root)) ||
			    Within(Mounts[Index].Directory, Mounts[Other].Directory) ||
			    Within(Mounts[Other].Directory, Mounts[Index].Directory))
			{
				throw std::invalid_argument("Conflicting package mounts or physical aliases");
			}
		}
	}
}

std::filesystem::path FMountedFileSystem::Normalize(const std::filesystem::path& InPath) const
{
	const auto Path = NormalizeFilePath(InPath);
	if (IsPackagePath(Path))
	{
		if (Path == "/")
		{
			throw std::invalid_argument("Package path requires a mount name");
		}
		const auto Relative = Path.relative_path();
		const auto Root = PathToUtf8(*Relative.begin());
		for (const auto& Mount : Mounts)
		{
			if (PathToUtf8(Mount.Root) == "/" + Root)
			{
				return Path;
			}
		}
		throw std::runtime_error("Unknown content mount: " + PathToUtf8(Path));
	}
	for (const auto& Mount : Mounts)
	{
		if (Within(Path, Mount.Directory))
		{
			auto Tail = PathToUtf8(Path).substr(PathToUtf8(Mount.Directory).size());
			if (Tail.starts_with('/'))
			{
				Tail.erase(0, 1);
			}
			return NormalizeFilePath(Mount.Root / PathFromUtf8(Tail));
		}
	}
	const auto Canonical = std::filesystem::weakly_canonical(Path);
	if (Canonical != Path)
	{
		return Normalize(Canonical);
	}
	return Path;
}

void FMountedFileSystem::ReplaceExclusive(FMountedFileSystem& InPrepared) noexcept
{
	Mounts.swap(InPrepared.Mounts);
}

std::vector<FDirectoryEntry> FMountedFileSystem::ListDirectory(const std::filesystem::path& InDirectory)
{
	const auto Directory = Normalize(InDirectory);
	const auto Physical = Resolve(Directory);
	auto Entries = Local.ListDirectory(Physical);
	for (auto& Entry : Entries)
	{
		Entry.Path = Directory / Entry.Path.filename();
	}
	return Entries;
}

std::filesystem::path FMountedFileSystem::Resolve(const std::filesystem::path& InPath, bool bInWrite) const
{
	const auto Path = Normalize(InPath);
	if (!IsPackagePath(Path))
	{
		return Path;
	}
	for (const auto& Mount : Mounts)
	{
		if (!Within(Path, Mount.Root))
		{
			continue;
		}
		if (bInWrite && Mount.bReadOnly)
		{
			throw std::runtime_error("Read-only content mount: " + PathToUtf8(Mount.Root));
		}
		const auto Relative = Path.lexically_relative(Mount.Root);
		const auto Physical = (Mount.Directory / Relative).lexically_normal();
		ValidateLocalPathCase(Mount.Directory, Relative);
		return Physical;
	}
	throw std::runtime_error("Unknown content mount: " + PathToUtf8(Path));
}

FBytes FMountedFileSystem::Read(const std::filesystem::path& InPath, std::size_t InLimit)
{
	return Local.Read(Resolve(InPath), InLimit);
}

FBytes FMountedFileSystem::ReadRange(const std::filesystem::path& InPath, std::size_t InOffset, std::size_t InSize)
{
	return Local.ReadRange(Resolve(InPath), InOffset, InSize);
}

void FMountedFileSystem::Remove(const std::filesystem::path& InPath)
{
	Local.Remove(Resolve(InPath, true));
}

std::vector<FFileContents> FMountedFileSystem::ReadTree(const std::filesystem::path& InDirectory,
                                                        std::span<const std::string_view> InExtensions,
                                                        std::size_t InLimit)
{
	const auto Directory = Normalize(InDirectory);
	if (!IsPackagePath(Directory))
	{
		return IFileSystem::ReadTree(Directory, InExtensions, InLimit);
	}
	const auto Physical = Resolve(Directory);
	std::vector<FFileContents> Result;
	// Enumeration already provides exact names and rejects descendant filesystem links.
	for (const auto& Path : EnumerateLocalContent(Physical, true))
	{
		if (InExtensions.empty() ||
		    std::find(InExtensions.begin(), InExtensions.end(), Path.extension().string()) != InExtensions.end())
		{
			Result.push_back({Directory / Path.lexically_relative(Physical), Local.Read(Path, InLimit)});
		}
	}
	return Result;
}

void FMountedFileSystem::WriteAtomic(const std::filesystem::path& InPath, std::span<const std::byte> InBytes)
{
	Local.WriteAtomic(Resolve(InPath, true), InBytes);
}

std::shared_ptr<IFileWriteLease> FMountedFileSystem::AcquireWriteLease(const std::filesystem::path& InPath)
{
	return Local.AcquireWriteLease(Resolve(InPath, true));
}

bool FMountedFileSystem::Exists(const std::filesystem::path& InPath)
{
	return Local.Exists(Resolve(InPath));
}

std::vector<std::filesystem::path> FMountedFileSystem::Enumerate(const std::filesystem::path& InDirectory,
                                                                 bool bInRecursive)
{
	const auto Physical = Resolve(InDirectory);
	const auto Directory = Normalize(InDirectory);
	const bool bMounted = IsPackagePath(Directory);
	auto Files = bMounted ? EnumerateLocalContent(Physical, bInRecursive) : Local.Enumerate(Physical, bInRecursive);
	for (auto& File : Files)
	{
		File = bMounted ? Directory / File.lexically_relative(Physical) : Normalize(File);
	}
	std::sort(Files.begin(), Files.end());
	return Files;
}
} // namespace Hyperion
