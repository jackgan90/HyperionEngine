#include "Hyperion/AssetImport/SkyImport.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/IO/Path.h"

namespace Hyperion
{
namespace
{
std::shared_ptr<void> ImportSky(FAssetImportContext& InContext)
{
	const auto& Bytes = InContext.Bytes;
	const auto Settings = InContext.Settings.Sky.value_or(FEnvironmentBakeSettings{});
	std::string Name = PathToUtf8(InContext.Path.stem());
	InContext.Cancellation.Check();
	const auto Image = DecodeHdrImage(*Bytes);
	if (Image.Width != 2ULL * Image.Height)
	{
		throw std::invalid_argument("Sky image is " + std::to_string(Image.Width) + " x " +
		                            std::to_string(Image.Height) +
		                            ". Choose a 2:1 HDR/EXR panorama (width must be twice the height).");
	}
	InContext.SourceWidth = Image.Width;
	InContext.SourceHeight = Image.Height;
	InContext.EffectiveSky = Settings;
	auto Baked = BakeEnvironment(Image.Rgba, Image.Width, Image.Height, Settings);
	InContext.Cancellation.Check();
	auto Result = std::make_shared<FSkyAsset>();
	Result->Name = std::move(Name);
	Baked.Radiance.Name = Result->Name + " radiance";
	Baked.Specular.Name = Result->Name + " specular";
	Result->Radiance = InContext.Emit("radiance", std::move(Baked.Radiance));
	Result->Specular = InContext.Emit("specular", std::move(Baked.Specular));
	if (std::dynamic_pointer_cast<FMountedFileSystem>(InContext.IO.FileSystem()))
	{
		Result->Brdf = {"", "/Engine/Textures/EnvironmentBrdf.hasset", RecordType<FTextureAsset>().Id, ""};
	}
	else
	{
		static const auto Brdf = BuildEnvironmentBrdf();
		Result->Brdf = InContext.Emit("brdf", Brdf, "builtin/environment-brdf-ggx-smith-v1");
	}
	Result->Irradiance = Baked.Irradiance;
	ValidateSkyAsset(*Result);
	return Result;
}
} // namespace

FAssetImporter MakeSkyImporter()
{
	return {"hyperion.sky-environment",
	        1,
	        &RecordType<FSkyAsset>(),
	        {".hdr", ".exr"},
	        ImportSky,
	        EAssetImporterExposure::Workspace,
	        "2:1 HDR/EXR panorama with sky bake settings.",
	        {false, true}};
}

void RegisterSkyImporter(FAssetImportService& InImports)
{
	InImports.Register(MakeSkyImporter());
}
} // namespace Hyperion
