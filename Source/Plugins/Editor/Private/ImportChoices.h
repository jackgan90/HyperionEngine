#pragma once
#include "Hyperion/AssetImport/ImportWorkspace.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Platform/FileDialog.h"
#include <algorithm>
#include <cctype>

namespace Hyperion
{
struct FImportTypeChoice
{
	std::string Type;
	std::string Label;
	std::vector<std::string> Extensions;

	bool SupportsSource(const std::string& InSource) const
	{
		auto Extension = PathToUtf8(PathFromUtf8(InSource).extension());
		std::transform(Extension.begin(), Extension.end(), Extension.begin(),
		               [](unsigned char InValue)
		               {
			               return static_cast<char>(std::tolower(InValue));
		               });
		return std::find(Extensions.begin(), Extensions.end(), Extension) != Extensions.end();
	}
};

inline std::vector<FImportTypeChoice> ImportTypeChoices(const FImportCapabilities& InCapabilities)
{
	std::vector<FImportTypeChoice> Choices;
	for (const auto& Format : InCapabilities.Formats)
	{
		const auto Label = Format.Type == RecordType<FModelAsset>().Id     ? "Model"
		                   : Format.Type == RecordType<FTextureAsset>().Id ? "Texture"
		                   : Format.Type == RecordType<FSkyAsset>().Id     ? "Sky"
		                                                                   : "";
		if (*Label)
		{
			Choices.push_back({Format.Type, Label, Format.Extensions});
		}
	}
	return Choices;
}

inline const FImportTypeChoice* ResolveImportChoice(const std::vector<FImportTypeChoice>& InChoices,
                                                    const std::string& InType, const std::string& InSource)
{
	const auto Found =
	    std::find_if(InChoices.begin(), InChoices.end(),
	                 [&](const auto& InChoice)
	                 {
		                 return InType.empty() ? InChoice.SupportsSource(InSource) : InChoice.Type == InType;
	                 });
	return Found == InChoices.end() ? nullptr : &*Found;
}

inline std::vector<FFileDialogFilter> ImportSourceFilters(const std::vector<FImportTypeChoice>& InChoices,
                                                          const std::string& InType)
{
	std::vector<FFileDialogFilter> Filters;
	std::string AllPatterns;
	for (const auto& Choice : InChoices)
	{
		if (!InType.empty() && Choice.Type != InType)
		{
			continue;
		}
		std::string Patterns;
		for (const auto& Extension : Choice.Extensions)
		{
			Patterns += (Patterns.empty() ? "" : ";") + std::string("*") + Extension;
		}
		if (!Patterns.empty())
		{
			AllPatterns += (AllPatterns.empty() ? "" : ";") + Patterns;
			Filters.push_back({Choice.Label, std::move(Patterns)});
		}
	}
	if (InType.empty() && !AllPatterns.empty())
	{
		Filters.insert(Filters.begin(), {"Supported assets", std::move(AllPatterns)});
	}
	return Filters;
}

inline FImportRequest ImportSettingsSnapshot(FImportRequest InRequest, const std::vector<FImportTypeChoice>& InChoices,
                                             EMaterialTextureEncoding InEncoding,
                                             const FEnvironmentBakeSettings& InBake)
{
	const auto* Choice = ResolveImportChoice(InChoices, InRequest.Type, InRequest.Source);
	const bool bModel = Choice && Choice->Type == RecordType<FModelAsset>().Id;
	const bool bTexture =
	    Choice && Choice->Type == RecordType<FTextureAsset>().Id && Choice->SupportsSource(InRequest.Source);
	const bool bSky = Choice && Choice->Type == RecordType<FSkyAsset>().Id && Choice->SupportsSource(InRequest.Source);
	InRequest.bScene &= bModel;
	if (!bModel && !bTexture)
	{
		InRequest.Name.clear();
	}
	InRequest.TextureEncoding = bTexture ? std::optional(InEncoding) : std::nullopt;
	InRequest.Sky = bSky ? std::optional(InBake) : std::nullopt;
	return InRequest;
}
} // namespace Hyperion
