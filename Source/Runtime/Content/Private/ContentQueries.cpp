#include "Hyperion/Content/ContentQueries.h"
#include <algorithm>
#include <cctype>

namespace Hyperion
{
FContentAssetPage QueryContentAssets(const FAssetService& InAssets, const FContentRootService& InRoots,
                                     const FContentAssetQuery& InRequest)
{
	const auto Generation = InRoots.Info().Generation;
	if (InRequest.Generation != Generation)
	{
		throw FContentRootError("stale_revision",
		                        "Content root changed; restart discovery with its current generation");
	}
	if (!InRequest.Limit || InRequest.Limit > 100 || InRequest.Query.size() > 256)
	{
		throw std::invalid_argument("Limit must be 1-100 and query at most 256 bytes");
	}
	const auto Fold = [](std::string InText)
	{
		std::transform(InText.begin(), InText.end(), InText.begin(),
		               [](unsigned char InValue)
		               {
			               return static_cast<char>(std::tolower(InValue));
		               });
		return InText;
	};
	auto Entries = InAssets.GetAssetIndex();
	const auto Query = Fold(InRequest.Query);
	std::erase_if(Entries,
	              [&](const FAssetRef& InEntry)
	              {
		              return (!InRequest.Type.empty() && InEntry.TypeId != InRequest.Type) ||
		                     (!Query.empty() && Fold(InEntry.Path + " " + InEntry.Id).find(Query) == std::string::npos);
	              });
	std::sort(Entries.begin(), Entries.end(),
	          [](const FAssetRef& InLeft, const FAssetRef& InRight)
	          {
		          return std::tie(InLeft.Path, InLeft.Id) < std::tie(InRight.Path, InRight.Id);
	          });
	FContentAssetPage Result{Generation, Entries.size()};
	const auto End = std::min(Entries.size(), std::size_t(InRequest.Offset) + InRequest.Limit);
	for (std::size_t Index = InRequest.Offset; Index < End; ++Index)
	{
		Result.Assets.push_back(Entries[Index]);
	}
	if (End < Entries.size())
	{
		Result.Next = static_cast<std::uint32_t>(End);
	}
	return Result;
}

template<> const FRecordDescriptor& RecordType<FContentAssetQuery>()
{
	static const auto Type = MakeRecord<FContentAssetQuery>(
	    "hyperion.content.assets.query",
	    {Member("generation", &FContentAssetQuery::Generation,
	            {.bRequired = true,
	             .Description = "Root generation from content.root.get. Restart pagination after content edits."}),
	     Member("query", &FContentAssetQuery::Query,
	            {.Description = "Case-insensitive path/identity substring, at most 256 bytes."}),
	     Member("type", &FContentAssetQuery::Type,
	            {.Description = "Optional exact reflected asset type ID, e.g. hyperion.scene."}),
	     Member("offset", &FContentAssetQuery::Offset), Member("limit", &FContentAssetQuery::Limit)});
	return Type;
}

template<> const FRecordDescriptor& RecordType<FContentAssetPage>()
{
	static const auto Type = MakeRecord<FContentAssetPage>(
	    "hyperion.content.assets.page",
	    {Member("generation", &FContentAssetPage::Generation), Member("total", &FContentAssetPage::Total),
	     Member("next", &FContentAssetPage::Next), Member("assets", &FContentAssetPage::Assets)});
	return Type;
}
} // namespace Hyperion
