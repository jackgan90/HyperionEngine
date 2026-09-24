#include "Hyperion/Content/ContentQueries.h"
#include "Hyperion/IO/Path.h"
#include <algorithm>

namespace Hyperion
{
FContentDirectoryPage QueryContentDirectory(const FAssetService& InAssets, const FContentRootService& InRoots,
                                            const FContentDirectoryQuery& InRequest)
{
	const auto Root = InRoots.Info();
	if (InRequest.Generation != Root.Generation)
	{
		throw FContentRootError("stale_revision", "Content root changed; query content.root.get");
	}
	if (!InRequest.Limit || InRequest.Limit > 100)
	{
		throw std::invalid_argument("Directory page limit must be 1-100");
	}
	const auto& Path = InRequest.Directory;
	if (!(Path == "/Game" || Path.starts_with("/Game/") || Path == "/Engine" || Path.starts_with("/Engine/")) ||
	    Path.find('\\') != std::string::npos)
	{
		throw std::invalid_argument("Directory must be an absolute /Game or /Engine package path");
	}
	const auto Directory = PathFromUtf8(Path);
	for (const auto& Part : Directory)
	{
		if (Part == "..")
		{
			throw std::invalid_argument("Parent traversal is not allowed");
		}
	}
	if (Root.Directory.empty() && (Path == "/Game" || Path.starts_with("/Game/")))
	{
		throw FContentRootError("root_unset", "Select Game content with content.root.set");
	}
	auto Entries = InAssets.FileSystem()->ListDirectory(Directory);
	std::sort(Entries.begin(), Entries.end(),
	          [](const auto& InLeft, const auto& InRight)
	          {
		          return InLeft.Path < InRight.Path;
	          });
	std::map<std::string, FAssetRef> Indexed;
	for (const auto& Asset : InAssets.GetAssetIndex())
	{
		Indexed.emplace(Asset.Path, Asset);
	}
	FContentDirectoryPage Result{Root.Generation, Entries.size()};
	const auto Begin = std::min(std::size_t(InRequest.Offset), Entries.size());
	const auto End = std::min(Begin + InRequest.Limit, Entries.size());
	for (auto Index = Begin; Index < End; ++Index)
	{
		const auto& Entry = Entries[Index];
		FContentCandidate Item{PathToUtf8(Entry.Path), "file", Entry.Error};
		if (!Entry.Error.empty())
		{
			Item.State = "inaccessible";
		}
		else if (Entry.bDirectory)
		{
			Item.State = "directory";
		}
		else if (const auto Asset = Indexed.find(Item.Path); Asset != Indexed.end())
		{
			Item.State = "indexed";
			Item.Asset = Asset->second;
		}
		else if (Entry.Path.extension() == ".hasset")
		{
			Item.State = "native_unindexed";
		}
		Result.Entries.push_back(std::move(Item));
	}
	if (End < Entries.size())
	{
		Result.Next = static_cast<std::uint32_t>(End);
	}
	return Result;
}

template<> const FRecordDescriptor& RecordType<FContentDirectoryQuery>()
{
	static const auto Type = MakeRecord<FContentDirectoryQuery>(
	    "hyperion.content.directory.query",
	    {Member("generation", &FContentDirectoryQuery::Generation,
	            {.bRequired = true,
	             .Description = "Current content.root.get generation. Restart pages after root/content changes."}),
	     Member("directory", &FContentDirectoryQuery::Directory,
	            {.Description =
	                 "Direct children of /Game or /Engine package directory; no physical paths or parent traversal."}),
	     Member("offset", &FContentDirectoryQuery::Offset),
	     Member("limit", &FContentDirectoryQuery::Limit,
	            {.Description = "Page size 1-100; listing is not a filesystem snapshot."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FContentCandidate>()
{
	static const auto Type = MakeRecord<FContentCandidate>(
	    "hyperion.content.candidate",
	    {Member("path", &FContentCandidate::Path),
	     Member(
	         "state", &FContentCandidate::State,
	         {.Description =
	              "directory, file, indexed, native_unindexed, or inaccessible. Unindexed is not proof of corruption; "
	              "asset.open supplies authoritative load diagnostics. Indexed does not imply payload validity."}),
	     Member("error", &FContentCandidate::Error, {.Description = "Directory enumeration/access error, if any."}),
	     Member("asset", &FContentCandidate::Asset, {.Description = "Current index reference, when recognized."})});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FContentDirectoryPage>()
{
	static const auto Type = MakeRecord<FContentDirectoryPage>(
	    "hyperion.content.directory.page",
	    {Member("generation", &FContentDirectoryPage::Generation), Member("total", &FContentDirectoryPage::Total),
	     Member("next", &FContentDirectoryPage::Next), Member("entries", &FContentDirectoryPage::Entries)});
	return Type;
}
} // namespace Hyperion
