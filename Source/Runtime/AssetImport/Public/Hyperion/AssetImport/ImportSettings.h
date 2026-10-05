#pragma once
#include "Hyperion/Environment/SkyAsset.h"

namespace Hyperion
{
struct FAssetImporter;
struct FAssetConversionSettings;
void ValidateImportSettings(const FAssetConversionSettings& InSettings, const FAssetImporter* InImporter);

struct FAssetConversionSettings
{
	std::optional<EMaterialTextureEncoding> TextureEncoding;
	std::optional<FEnvironmentBakeSettings> Sky;
};

void ValidateImportSettings(const FAssetConversionSettings& InSettings, std::string_view InExtension,
                            std::string_view InType);
template<> const FRecordDescriptor& RecordType<FEnvironmentBakeSettings>();
} // namespace Hyperion
