#pragma once
#include "Hyperion/Assets/NativeAsset.h"
#include "Hyperion/IO/IOService.h"
#include <map>
#include <string_view>

namespace Hyperion
{
struct FContentDirectory
{
	std::vector<FDirectoryEntry> Entries;
	std::string Error;
};

struct FContentScenes
{
	std::vector<std::string> Paths;
	std::string Error;
};

std::string ContentRelativePath(std::string_view InPath);
bool IsBrowserEntry(const FDirectoryEntry& InEntry, bool bInInternal);
FContentDirectory ReadContentDirectory(IFileSystem& InFiles, const std::filesystem::path& InPath, bool bInInternal);
FContentScenes DiscoverContentScenes(FIOService& InIO, bool bInInternal, FCancellationToken InCancellation = {});
FAssetHeader ReadContentHeader(FIOService& InIO, const std::filesystem::path& InPath,
                               FCancellationToken InCancellation = {});

class FContentBrowser
{
public:
	explicit FContentBrowser(FIOService& InIO) : IO(InIO)
	{
	}

	~FContentBrowser();
	void Refresh(bool bInMounted, bool bInInternal);
	void Stop();
	void Navigate(const std::string& InPath);
	const FContentDirectory* Directory(const std::string& InPath);
	std::optional<FContentScenes> PollScenes();

	bool IsScanning() const
	{
		return Scenes.has_value();
	}

	std::string SelectedDirectory = "/Game";
	std::string SelectedFile;

private:
	FIOService& IO;
	FCancellationToken Cancellation;
	bool bInternal{};
	bool bMounted{};
	std::map<std::string, TAsyncResult<FContentDirectory>> Directories;
	std::optional<TAsyncResult<FContentScenes>> Scenes;
	std::string DirectoryToRefresh;
};
} // namespace Hyperion
