#include "Hyperion/AssetImport/ImportSettings.h"

namespace Hyperion
{
void ValidateImportSettings(const FAssetConversionSettings& InSettings, std::string_view InExtension,
                            std::string_view InType)
{
	if (InSettings.TextureEncoding)
	{
		if (*InSettings.TextureEncoding > EMaterialTextureEncoding::Srgb ||
		    (InExtension != ".png" && InExtension != ".jpg" && InExtension != ".jpeg") ||
		    (!InType.empty() && InType != RecordType<FTextureAsset>().Id))
		{
			throw std::invalid_argument("Texture encoding applies only to standalone PNG/JPEG imports");
		}
	}
	if (InSettings.Sky)
	{
		if ((InExtension != ".hdr" && InExtension != ".exr") ||
		    (!InType.empty() && InType != RecordType<FSkyAsset>().Id))
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
