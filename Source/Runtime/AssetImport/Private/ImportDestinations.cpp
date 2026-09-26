#include "AssetImportInternal.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/Path.h"
#include <set>

namespace Hyperion
{
namespace
{
bool MatchesImportFolder(IFileSystem& InFiles, const std::filesystem::path& InDirectory, const std::string& InKey)
{
	try
	{
		const auto Bytes = InFiles.Read(InDirectory / ".import-source", InKey.size());
		return Bytes == FBytes(std::as_bytes(std::span(InKey)).begin(), std::as_bytes(std::span(InKey)).end());
	}
	catch (const std::exception&)
	{
		// An unreadable or foreign marker never grants ownership; the directory stays occupied.
		return false;
	}
}
} // namespace

std::vector<FDirectoryEntry> ImportDirectoryEntries(IFileSystem& InFiles, const std::filesystem::path& InDirectory)
{
	try
	{
		return InFiles.ListDirectory(InDirectory);
	}
	catch (const std::filesystem::filesystem_error& Failure)
	{
		if (Failure.code() != std::errc::no_such_file_or_directory)
		{
			throw;
		}
		return {};
	}
}

std::string ImportFolderKey(FIOService& InIO, const std::filesystem::path& InSource,
                            const std::filesystem::path& InOutput)
{
	const auto Identity =
	    ImportPathString(InIO.FileSystem()->Normalize(InSource)) + "\n" + ImportPathString(InOutput.filename());
	return ContentHash(std::as_bytes(std::span(Identity)));
}

std::filesystem::path SelectImportFolderOutput(FIOService& InIO, const std::filesystem::path& InSource,
                                               const std::filesystem::path& InOutput)
{
	auto Stem = ImportPathString(InSource.stem());
	while (!Stem.empty() && (Stem.back() == '.' || Stem.back() == ' '))
	{
		Stem.pop_back();
	}
	if (Stem.empty() || Stem == "." || Stem == "..")
	{
		Stem = "Asset";
	}
	const auto Key = ImportFolderKey(InIO, InSource, InOutput);
	return *DispatchAsync<std::filesystem::path>(
	            InIO.TaskSystem(), {EDomain::Io},
	            [Files = InIO.FileSystem(), Stem, Key, InOutput]
	            {
		            const auto Entries = ImportDirectoryEntries(*Files, InOutput.parent_path());
		            std::set<std::string> Occupied;
		            std::optional<std::filesystem::path> Previous;
		            for (const auto& Entry : Entries)
		            {
			            Occupied.insert(PathCaseKey(Entry.Path));
			            if (!Entry.bDirectory || !Entry.Error.empty() || !MatchesImportFolder(*Files, Entry.Path, Key))
			            {
				            continue;
			            }
			            if (Previous)
			            {
				            throw std::runtime_error("Multiple folders belong to this import source");
			            }
			            Previous = Entry.Path / InOutput.filename();
		            }

		            if (Previous)
		            {
			            return *Previous;
		            }
		            for (std::uint32_t Index = 0; Index < 10000; ++Index)
		            {
			            const auto Name = Stem + (Index ? "_" + std::to_string(Index) : "");
			            const auto Directory = InOutput.parent_path() / PathFromUtf8(Name);
			            if (!Occupied.contains(PathCaseKey(Directory)) && !Files->Exists(Directory))
			            {
				            return Directory / InOutput.filename();
			            }
		            }
		            throw std::runtime_error("No available import folder name");
	            })
	            .Get(InIO.TaskSystem());
}

FAssetImportResult FAssetImportService::FImpl::PublishGrouped(const std::filesystem::path& InSource,
                                                              const std::filesystem::path& InOutput,
                                                              const FAssetImportOptions& InOptions)
{
	const auto ParentLease =
	    IO.AcquireWriteLeaseAsync(InOutput.parent_path() / ".publish-library", Cancellation).Get(IO.TaskSystem());
	const auto Output = SelectImportFolderOutput(IO, InSource, InOutput);
	auto Options = InOptions;
	Options.bCreateFolder = false;
	Options.FolderKey = ImportFolderKey(IO, InSource, InOutput);
	Options.Library = Output.parent_path();
	const bool bNewDirectory = !*DispatchAsync<bool>(IO.TaskSystem(), {EDomain::Io},
	                                                 [Files = IO.FileSystem(), Directory = Output.parent_path()]
	                                                 {
		                                                 return Files->Exists(Directory);
	                                                 })
	                                 .Get(IO.TaskSystem());
	try
	{
		return Publish(InSource, Output, Options);
	}
	catch (const std::exception& Failure)
	{
		if (bNewDirectory)
		{
			try
			{
				DispatchAsync<bool>(IO.TaskSystem(), {EDomain::Io},
				                    [Files = IO.FileSystem(), Directory = Output.parent_path()]
				                    {
					                    return Files->RemoveEmptyDirectory(Directory);
				                    })
				    .Get(IO.TaskSystem());
			}
			catch (const std::exception& Cleanup)
			{
				throw std::runtime_error(std::string(Failure.what()) +
				                         "\nEmpty import folder cleanup failed: " + Cleanup.what());
			}
		}
		throw;
	}
}

} // namespace Hyperion
