#include "Hyperion/AssetImport/ImportSettings.h"
#include <bit>

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
		const auto& Settings = *InSettings.Sky;
		if ((InExtension != ".hdr" && InExtension != ".exr") ||
		    (!InType.empty() && InType != RecordType<FSkyAsset>().Id))
		{
			throw std::invalid_argument("Sky bake settings apply only to HDR/EXR panoramas");
		}
		if (!std::has_single_bit(Settings.RadianceSize) || Settings.RadianceSize > 1024 ||
		    !std::has_single_bit(Settings.SpecularSize) || Settings.SpecularSize > 256 ||
		    Settings.SpecularSize > Settings.RadianceSize || !Settings.Samples || Settings.Samples > 1024)
		{
			throw std::invalid_argument(
			    "Sky sizes must be powers of two: radiance 1-1024, specular 1-256 and <= radiance; samples 1-1024");
		}
	}
}

template<> const FRecordDescriptor& RecordType<FEnvironmentBakeSettings>()
{
	static const auto Type = MakeRecord<FEnvironmentBakeSettings>(
	    "asset.import.sky-settings",
	    {Member("radianceSize", &FEnvironmentBakeSettings::RadianceSize,
	            {.Description = "Cube face size; power of two from 1 to 1024. Default 256."}),
	     Member("specularSize", &FEnvironmentBakeSettings::SpecularSize,
	            {.Description =
	                 "Prefiltered face size; power of two from 1 to 256, no larger than radianceSize. Default 64."}),
	     Member("samples", &FEnvironmentBakeSettings::Samples,
	            {.Description = "GGX samples from 1 to 1024. Default 256."})});
	return Type;
}
} // namespace Hyperion
