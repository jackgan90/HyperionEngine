#include "AssetPublicationInternal.h"

namespace Hyperion
{
void FPublication::Commit()
{
	CheckSources();
	std::map<std::filesystem::path, std::optional<FBytes>> PreviousBytes;
	std::vector<std::filesystem::path> Changed;
	const auto Marker = Output.parent_path() / ".import-source";
	const auto KeyBytes = std::as_bytes(std::span(FolderKey));
	const FBytes MarkerBytes(KeyBytes.begin(), KeyBytes.end());
	for (const auto& [Path, Asset] : Staged)
	{
		const auto Existing = IO.TryReadAsync(Path, Cancellation).Get(IO.TaskSystem());
		if (*Existing && **Existing == Asset.Bytes)
		{
			continue;
		}
		PreviousBytes.emplace(Path, *Existing);
		if (Path != Output)
		{
			Changed.push_back(Path);
		}
	}
	if (!FolderKey.empty())
	{
		const auto Existing = IO.TryReadAsync(Marker, Cancellation).Get(IO.TaskSystem());
		if (*Existing && **Existing != MarkerBytes)
		{
			throw std::runtime_error("Import folder ownership changed before publication");
		}
		if (!*Existing)
		{
			PreviousBytes.emplace(Marker, *Existing);
			Changed.push_back(Marker);
		}
	}
	if (PreviousBytes.contains(Output))
	{
		Changed.push_back(Output);
	}
	std::vector<std::filesystem::path> Attempted;
	try
	{
		for (const auto& Path : Changed)
		{
			Cancellation.Check();
			Attempted.push_back(Path);
			IO.WriteAsync(Path, Path == Marker ? MarkerBytes : Staged.at(Path).Bytes, Cancellation)
			    .Get(IO.TaskSystem());
			if (Path != Marker)
			{
				++Written;
			}
		}
	}
	catch (const std::exception& Failure)
	{
		std::string Message = Failure.what();
		for (auto It = Attempted.rbegin(); It != Attempted.rend(); ++It)
		{
			try
			{
				const auto& Backup = PreviousBytes.at(*It);
				if (Backup)
				{
					IO.WriteAsync(*It, *Backup).Get(IO.TaskSystem());
				}
				else
				{
					DispatchAsync<bool>(IO.TaskSystem(), {EDomain::Io},
					                    [Files = IO.FileSystem(), Path = *It]
					                    {
						                    Files->Remove(Path);
						                    return true;
					                    })
					    .Get(IO.TaskSystem());
				}
			}
			catch (const std::exception& Rollback)
			{
				Message += "\nRollback failed for " + ImportPathString(*It) + ": " + Rollback.what();
			}
		}
		throw std::runtime_error(Message);
	}
}
} // namespace Hyperion
