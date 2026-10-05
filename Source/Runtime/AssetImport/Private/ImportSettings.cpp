#include "Hyperion/AssetImport/ImportSettings.h"
#include "Hyperion/AssetImport/ImporterRegistry.h"

namespace Hyperion
{
void ValidateImportSettings(const FAssetConversionSettings& InSettings, std::string_view InExtension,
                            std::string_view InType)
{
	const auto Importers = DefaultAssetImporters();
	ValidateImportSettings(InSettings, FindAssetImporter(Importers, InExtension, InType));
}

void ValidateImportSettings(const FAssetConversionSettings& InSettings, const FAssetImporter* InImporter)
{
	if (InSettings.TextureEncoding)
	{
		if (*InSettings.TextureEncoding > EMaterialTextureEncoding::Srgb || !InImporter ||
		    !InImporter->Settings.bTextureEncoding)
		{
			throw std::invalid_argument("Texture encoding applies only to standalone PNG/JPEG imports");
		}
	}
	if (InSettings.Sky)
	{
		if (!InImporter || !InImporter->Settings.bSkyBake)
		{
			throw std::invalid_argument("Sky bake settings apply only to HDR/EXR panoramas");
		}
		if (!IsValidEnvironmentBakeSettings(*InSettings.Sky))
		{
			throw std::invalid_argument("Sky sizes must be powers of two: radiance 1-" +
			                            std::to_string(FEnvironmentBakeLimits::MaxRadianceSize) + ", specular 1-" +
			                            std::to_string(FEnvironmentBakeLimits::MaxSpecularSize) +
			                            " and <= radiance; samples 1-" +
			                            std::to_string(FEnvironmentBakeLimits::MaxSamples));
		}
	}
}

template<> const FRecordDescriptor& RecordType<FEnvironmentBakeSettings>()
{
	const FEnvironmentBakeSettings Defaults;
	static const auto Type = MakeRecord<FEnvironmentBakeSettings>(
	    "asset.import.sky-settings",
	    {Member("radianceSize", &FEnvironmentBakeSettings::RadianceSize,
	            {.Description = "Cube face size; power of two from 1 to " +
	                            std::to_string(FEnvironmentBakeLimits::MaxRadianceSize) + ". Default " +
	                            std::to_string(Defaults.RadianceSize) + "."}),
	     Member("specularSize", &FEnvironmentBakeSettings::SpecularSize,
	            {.Description = "Prefiltered face size; power of two from 1 to " +
	                            std::to_string(FEnvironmentBakeLimits::MaxSpecularSize) +
	                            ", no larger than radianceSize. Default " + std::to_string(Defaults.SpecularSize) +
	                            "."}),
	     Member("samples", &FEnvironmentBakeSettings::Samples,
	            {.Description = "GGX samples from 1 to " + std::to_string(FEnvironmentBakeLimits::MaxSamples) +
	                            ". Default " + std::to_string(Defaults.Samples) + "."})});
	return Type;
}
} // namespace Hyperion
