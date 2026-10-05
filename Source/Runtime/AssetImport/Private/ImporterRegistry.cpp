#include "Hyperion/AssetImport/ImporterRegistry.h"
#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/ImageImport.h"
#include "Hyperion/AssetImport/SkyImport.h"
#include <algorithm>
#include <cctype>
#include <set>

namespace Hyperion
{
FAssetImporter NormalizeAssetImporter(FAssetImporter InImporter)
{
	if (InImporter.Id.empty() || InImporter.Id.size() > 128 ||
	    InImporter.Id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-") != std::string::npos ||
	    !InImporter.Version || !InImporter.Type || !InImporter.Convert || InImporter.Extensions.empty() ||
	    (InImporter.Exposure != EAssetImporterExposure::ToolingOnly &&
	     InImporter.Exposure != EAssetImporterExposure::Workspace) ||
	    (InImporter.Exposure == EAssetImporterExposure::Workspace && InImporter.Description.empty()))
	{
		throw std::logic_error("Register valid source importers before conversion");
	}
	ValidateRecordDescriptor(*InImporter.Type);
	std::set<std::string> Extensions;
	for (auto& Extension : InImporter.Extensions)
	{
		std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		               [](unsigned char InCharacter)
		               {
			               return static_cast<char>(std::tolower(InCharacter));
		               });
		if (Extension.size() < 2 || Extension.front() != '.' ||
		    Extension.substr(1).find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos ||
		    !Extensions.insert(Extension).second)
		{
			throw std::invalid_argument("Invalid or duplicate source extension: " + Extension);
		}
		if (Extension == ".hasset")
		{
			throw std::invalid_argument("Native assets are not import sources");
		}
	}
	InImporter.OwnedType = std::make_shared<const FRecordDescriptor>(*InImporter.Type);
	InImporter.Type = InImporter.OwnedType.get();
	return InImporter;
}

const FAssetImporter* FindAssetImporter(std::span<const FAssetImporter> InImporters, std::string_view InExtension,
                                        std::string_view InType, bool bInWorkspaceInference)
{
	const auto Found = std::find_if(InImporters.begin(), InImporters.end(),
	                                [&](const auto& InImporter)
	                                {
		                                return (InType.empty() || InImporter.Type->Id == InType) &&
		                                       (!InType.empty() || !bInWorkspaceInference ||
		                                        InImporter.Exposure == EAssetImporterExposure::Workspace) &&
		                                       std::find(InImporter.Extensions.begin(), InImporter.Extensions.end(),
		                                                 InExtension) != InImporter.Extensions.end();
	                                });
	return Found == InImporters.end() ? nullptr : &*Found;
}

std::vector<FAssetImporter> DefaultAssetImporters()
{
	return {MakeGltfImporter(), MakeImageImporter(), MakeSkyImporter(), MakeGltfSourceImporter()};
}
} // namespace Hyperion
