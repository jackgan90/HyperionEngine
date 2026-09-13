#include "Hyperion/AssetImport/SkyImport.h"
#include "Hyperion/AssetImport/MaterialImport.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/IO/Path.h"
#include <nlohmann/json.hpp>
#include <set>

namespace Hyperion
{
namespace
{
std::shared_ptr<void> ImportSky(FAssetImportContext& InContext)
{
	auto Bytes = InContext.Bytes;
	FEnvironmentBakeSettings Settings;
	std::string Name = PathToUtf8(InContext.Path.stem());
	if (InContext.Path.extension() == ".json")
	{
		const std::string_view Text(reinterpret_cast<const char*>(Bytes->data()), Bytes->size());
		const auto Archive = DecodeAssetSourceJson(Text);
		const auto Json = nlohmann::json::parse(Text);
		if (Json.contains("fields"))
		{
			return ReadRecord(RecordType<FSkyAsset>(), Archive);
		}
		const std::set<std::string> Allowed{"type",          "schema_version", "name",   "source",
		                                    "radiance_size", "specular_size",  "samples"};
		for (const auto& [Key, Value] : Json.items())
		{
			if (!Allowed.contains(Key))
			{
				throw std::invalid_argument("Unsupported sky import field: " + Key);
			}
		}
		if (Json.at("type") != RecordType<FSkyAsset>().Id || Json.at("schema_version") != 1)
		{
			throw std::invalid_argument("Unsupported sky import descriptor");
		}
		Name = Json.value("name", Name);
		Settings.RadianceSize = Json.value("radiance_size", Settings.RadianceSize);
		Settings.SpecularSize = Json.value("specular_size", Settings.SpecularSize);
		Settings.Samples = Json.value("samples", Settings.Samples);
		Bytes = InContext.Read(InContext.Path.parent_path() / PathFromUtf8(Json.at("source").get<std::string>()));
	}
	InContext.Cancellation.Check();
	const auto Image = DecodeHdrImage(*Bytes);
	auto Baked = BakeEnvironment(Image.Rgba, Image.Width, Image.Height, Settings);
	InContext.Cancellation.Check();
	auto Result = std::make_shared<FSkyAsset>();
	Result->Name = std::move(Name);
	Baked.Radiance.Name = Result->Name + " radiance";
	Baked.Specular.Name = Result->Name + " specular";
	Result->Radiance = InContext.Emit("radiance", std::move(Baked.Radiance));
	Result->Specular = InContext.Emit("specular", std::move(Baked.Specular));
	static const auto Brdf = BuildEnvironmentBrdf();
	Result->Brdf = InContext.Emit("brdf", Brdf, "builtin/environment-brdf-ggx-smith-v1");
	Result->Irradiance = Baked.Irradiance;
	ValidateSkyAsset(*Result);
	return Result;
}
} // namespace

void RegisterSkyImporter(FAssetImportService& InImports)
{
	InImports.Register({"hyperion.sky-environment", 1, &RecordType<FSkyAsset>(), {".hdr", ".exr", ".json"}, ImportSky});
	InImports.Register({"hyperion.native-sky",
	                    1,
	                    &RecordType<FSkyAsset>(),
	                    {".hasset"},
	                    [](FAssetImportContext& InContext)
	                    {
		                    return ReadRecord(RecordType<FSkyAsset>(), DecodeAsset(InContext.Bytes).Object);
	                    }});
}
} // namespace Hyperion
