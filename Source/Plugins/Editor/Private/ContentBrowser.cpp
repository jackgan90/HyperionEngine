#include "ContentBrowser.h"
#include "Hyperion/Assets/AssetRegistry.h"
#include "Hyperion/IO/Path.h"
#include <algorithm>
#include <cctype>

namespace Hyperion
{
namespace
{
std::string FoldName(std::string InName)
{
	std::transform(InName.begin(), InName.end(), InName.begin(),
	               [](unsigned char InValue)
	               {
		               return static_cast<char>(std::tolower(InValue));
	               });
	return InName;
}
} // namespace

std::string ContentRelativePath(std::string_view InPath)
{
	if (InPath == "/Game")
	{
		return {};
	}
	return std::string(InPath.starts_with("/Game/") ? InPath.substr(6) : InPath);
}

bool IsBrowserEntry(const FDirectoryEntry& InEntry)
{
	const auto Name = FoldName(PathToUtf8(InEntry.Path.filename()));
	if (Name == ".git" || Name == ".cache" || Name.starts_with(".publish-"))
	{
		return false;
	}
	return InEntry.bDirectory || !InEntry.Error.empty() || FoldName(PathToUtf8(InEntry.Path.extension())) == ".hasset";
}

FContentDirectory ReadContentDirectory(IFileSystem& InFiles, const std::filesystem::path& InPath)
{
	FContentDirectory Result;
	try
	{
		for (auto Entry : InFiles.ListDirectory(InPath))
		{
			if (!IsBrowserEntry(Entry))
			{
				continue;
			}
			if (!Entry.Error.empty())
			{
				Result.Error += PathToUtf8(Entry.Path) + ": " + Entry.Error + "\n";
				continue;
			}
			Result.Entries.push_back(std::move(Entry));
		}
		std::sort(Result.Entries.begin(), Result.Entries.end(),
		          [](const auto& InA, const auto& InB)
		          {
			          if (InA.bDirectory != InB.bDirectory)
			          {
				          return InA.bDirectory;
			          }
			          return InA.Path.filename() < InB.Path.filename();
		          });
	}
	catch (const std::exception& Failure)
	{
		Result.Error = Failure.what();
	}
	return Result;
}

FAssetHeader ReadContentHeader(FIOService& InIO, const std::filesystem::path& InPath, FCancellationToken InCancellation)
{
	return *DispatchAsync<FAssetHeader>(
	            InIO.TaskSystem(), {EDomain::Io},
	            [Files = InIO.FileSystem(), InPath]
	            {
		            return ReadAssetHeader(*Files, InPath);
	            },
	            InCancellation)
	            .Get(InIO.TaskSystem());
}

FContentScenes DiscoverContentScenes(FIOService& InIO, FCancellationToken InCancellation)
{
	FContentScenes Result;
	std::vector<std::filesystem::path> Pending{"/Game"};
	while (!Pending.empty())
	{
		InCancellation.Check();
		const auto Path = std::move(Pending.back());
		Pending.pop_back();
		const auto Listing = DispatchAsync<FContentDirectory>(
		    InIO.TaskSystem(), {EDomain::Io},
		    [Files = InIO.FileSystem(), Path]
		    {
			    return ReadContentDirectory(*Files, Path);
		    },
		    InCancellation);
		const auto Directory = *Listing.Get(InIO.TaskSystem());
		if (Result.Error.size() < 4096)
		{
			Result.Error += Directory.Error;
		}
		for (const auto& Entry : Directory.Entries)
		{
			InCancellation.Check();
			if (Entry.bDirectory)
			{
				Pending.push_back(Entry.Path);
				continue;
			}
			try
			{
				if (ReadContentHeader(InIO, Entry.Path, InCancellation).TypeId == "hyperion.scene")
				{
					Result.Paths.push_back(PathToUtf8(Entry.Path));
				}
			}
			catch (const std::exception& Failure)
			{
				InCancellation.Check();
				if (Result.Error.size() < 4096)
				{
					Result.Error += PathToUtf8(Entry.Path) + ": " + Failure.what() + "\n";
				}
			}
		}
	}
	std::sort(Result.Paths.begin(), Result.Paths.end());
	return Result;
}

FContentBrowser::~FContentBrowser()
{
	Stop();
}

void FContentBrowser::Stop()
{
	Cancellation.Cancel();
	for (auto& [Path, Request] : Directories)
	{
		try
		{
			IO.TaskSystem().Wait(Request.Task());
		}
		catch (...)
		{
		}
	}
	if (Scenes)
	{
		try
		{
			IO.TaskSystem().Wait(Scenes->Task());
		}
		catch (...)
		{
		}
	}
	Directories.clear();
	DirectoryToRefresh.clear();
	Scenes.reset();
	bMounted = false;
}

void FContentBrowser::Refresh(bool bInMounted)
{
	Stop();
	Cancellation = {};
	bMounted = bInMounted;
	if (bMounted)
	{
		Scenes = DispatchAsync<FContentScenes>(
		    IO.TaskSystem(), {EDomain::Worker},
		    [Service = &IO, Token = Cancellation]
		    {
			    return DiscoverContentScenes(*Service, Token);
		    },
		    Cancellation);
	}
}

const FContentDirectory* FContentBrowser::Directory(const std::string& InPath)
{
	if (!bMounted)
	{
		return nullptr;
	}
	const auto It = Directories.find(InPath);
	if (It != Directories.end())
	{
		return It->second.Ready() ? It->second.GetReady().get() : nullptr;
	}
	Directories.emplace(InPath, DispatchAsync<FContentDirectory>(
	                                IO.TaskSystem(), {EDomain::Io},
	                                [Files = IO.FileSystem(), Path = PathFromUtf8(InPath)]
	                                {
		                                return ReadContentDirectory(*Files, Path);
	                                },
	                                Cancellation));
	return nullptr;
}

void FContentBrowser::Navigate(const std::string& InPath)
{
	SelectedDirectory = InPath;
	SelectedFile.clear();
	DirectoryToRefresh = InPath;
}

std::optional<FContentScenes> FContentBrowser::PollScenes()
{
	// Defer invalidation until the next frame, after the UI releases listing references.
	if (!DirectoryToRefresh.empty())
	{
		const auto It = Directories.find(DirectoryToRefresh);
		if (It != Directories.end() && It->second.Ready())
		{
			Directories.erase(It);
		}
		DirectoryToRefresh.clear();
	}
	if (!Scenes || !Scenes->Ready())
	{
		return {};
	}
	auto Result = *Scenes->GetReady();
	Scenes.reset();
	return Result;
}
} // namespace Hyperion
