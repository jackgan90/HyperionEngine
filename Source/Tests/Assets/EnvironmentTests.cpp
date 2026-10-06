#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/SkyImport.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/Path.h"
#include "Hyperion/Scene/SceneManifest.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>

using namespace Hyperion;

namespace
{
template<class T> void Rejects(T InAction)
{
	bool bRejected{};
	try
	{
		InAction();
	}
	catch (const std::exception&)
	{
		bRejected = true;
	}
	HYP_CHECK(bRejected);
}

void CheckNumerics()
{
	std::vector<float> Pixels(32 * 16 * 4, 1);
	for (std::size_t Index = 0; Index < Pixels.size(); Index += 4)
	{
		Pixels[Index] = 2;
		Pixels[Index + 2] = .25f;
	}
	const auto Baked = BakeEnvironment(Pixels, 32, 16, {16, 8, 64});
	const std::array<FVec3, 6> Axes{{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};
	for (unsigned Face = 0; Face < 6; ++Face)
	{
		HYP_CHECK(Dot(CubeDirection(Face, 0, 0), Axes[Face]) > .9999f);
		const auto E = EvaluateEnvironmentSh(Baked.Irradiance, Axes[Face]);
		HYP_CHECK(std::abs(E.X - 2 * std::numbers::pi_v<float>) < .002f);
		HYP_CHECK(std::abs(E.Y - std::numbers::pi_v<float>) < .002f);
		for (unsigned Level = 0; Level < Baked.Specular.Mips.size(); ++Level)
		{
			const auto Value = SampleEnvironment(Baked.Specular, Axes[Face], float(Level));
			HYP_CHECK(std::abs(Value.X - 2) < .002f && std::abs(Value.Z - .25f) < .002f);
		}
	}
	// A directional panorama verifies projection axes, including the longitudinal seam.
	for (unsigned Y = 0; Y < 16; ++Y)
	{
		for (unsigned X = 0; X < 32; ++X)
		{
			const float Phi = (float(X) + .5f) / 32 * 2 * std::numbers::pi_v<float> - std::numbers::pi_v<float>;
			const float Theta = (float(Y) + .5f) / 16 * std::numbers::pi_v<float>;
			const std::size_t Offset = (Y * 32 + X) * 4;
			Pixels[Offset] = .5f + .5f * std::sin(Theta) * std::cos(Phi);
			Pixels[Offset + 1] = .5f + .5f * std::cos(Theta);
			Pixels[Offset + 2] = .5f + .5f * std::sin(Theta) * std::sin(Phi);
		}
	}
	const auto Directional = BakeEnvironment(Pixels, 32, 16, {16, 8, 64});
	for (const auto Axis : Axes)
	{
		const auto Sample = SampleEnvironment(Directional.Radiance, Axis);
		HYP_CHECK(std::abs(Sample.X - (.5f + .5f * Axis.X)) < .025f);
		HYP_CHECK(std::abs(Sample.Y - (.5f + .5f * Axis.Y)) < .025f);
		HYP_CHECK(std::abs(Sample.Z - (.5f + .5f * Axis.Z)) < .025f);
	}
	const auto Blur = SampleEnvironment(Directional.Specular, {1, 0, 0}, 3);
	HYP_CHECK(Blur.X > .55f && Blur.X < .95f);
	Pixels[0] = std::numeric_limits<float>::infinity();
	Rejects(
	    [&]
	    {
		    BakeEnvironment(Pixels, 32, 16, {16, 8, 8});
	    });
	Rejects(
	    [&]
	    {
		    BakeEnvironment({}, 32, 16);
	    });
	Rejects(
	    [&]
	    {
		    BakeEnvironment(Pixels, 32, 16, {15, 8, 8});
	    });
	const auto Brdf = BuildEnvironmentBrdf(16, 128);
	for (const auto& Mip : Brdf.Mips)
	{
		for (std::size_t Index = 0; Index < std::size_t(Mip.Width) * Mip.Height; ++Index)
		{
			const auto Value = ReadTexturePixel(Mip, Brdf.Format, Index);
			HYP_CHECK(Value[0] >= 0 && Value[1] >= 0 && Value[0] + Value[1] < 1.08f);
		}
	}
}

template<class TAction> std::string InvalidMessage(TAction InAction)
{
	try
	{
		InAction();
	}
	catch (const std::invalid_argument& Error)
	{
		return Error.what();
	}
	throw std::runtime_error("Expected invalid_argument from environment admission");
}

void CheckBakeSettings()
{
	struct FCase
	{
		FEnvironmentBakeSettings Settings;
		bool bValid;
	};

	const std::array Cases{FCase{{1, 1, 1}, true},
	                       FCase{{1024, 256, 1024}, true},
	                       FCase{{4, 4, 3}, true},
	                       FCase{{256, 256, 1}, true},
	                       FCase{{0, 1, 1}, false},
	                       FCase{{3, 1, 1}, false},
	                       FCase{{2048, 1, 1}, false},
	                       FCase{{1024, 0, 1}, false},
	                       FCase{{1024, 3, 1}, false},
	                       FCase{{1024, 512, 1}, false},
	                       FCase{{1, 2, 1}, false},
	                       FCase{{1, 1, 0}, false},
	                       FCase{{1, 1, 1025}, false},
	                       FCase{{std::numeric_limits<std::uint32_t>::max(), 1, 1}, false},
	                       FCase{{1, std::numeric_limits<std::uint32_t>::max(), 1}, false},
	                       FCase{{1, 1, std::numeric_limits<std::uint32_t>::max()}, false}};
	const std::array<float, 8> Pixels{1, 1, 1, 1, 1, 1, 1, 1};
	for (const auto& Entry : Cases)
	{
		HYP_CHECK(IsValidEnvironmentBakeSettings(Entry.Settings) == Entry.bValid);
		const FAssetConversionSettings Conversion{.Sky = Entry.Settings};
		for (const auto Extension : {".hdr", ".exr"})
		{
			if (Entry.bValid)
			{
				ValidateImportSettings(Conversion, Extension, RecordType<FSkyAsset>().Id);
			}
			else
			{
				HYP_CHECK(
				    InvalidMessage(
				        [&]
				        {
					        ValidateImportSettings(Conversion, Extension, {});
				        }) ==
				    "Sky sizes must be powers of two: radiance 1-1024, specular 1-256 and <= radiance; samples 1-1024");
			}
		}
		if (!Entry.bValid)
		{
			HYP_CHECK(InvalidMessage(
			              [&]
			              {
				              (void)BakeEnvironment(Pixels, 2, 1, Entry.Settings);
			              }) == "Sky requires a 2:1 HDR panorama and bounded power-of-two bake sizes");
		}
		else if (Entry.Settings.RadianceSize <= 4)
		{
			const auto Baked = BakeEnvironment(Pixels, 2, 1, Entry.Settings);
			HYP_CHECK(Baked.Radiance.Mips.front().Width == Entry.Settings.RadianceSize);
			HYP_CHECK(Baked.Specular.Mips.front().Width == Entry.Settings.SpecularSize);
		}
	}
	HYP_CHECK(IsValidEnvironmentPrefilterSettings(2048, 256, 1024));
	HYP_CHECK(IsValidEnvironmentPrefilterSettings(3, 2, 1));
	HYP_CHECK(!IsValidEnvironmentPrefilterSettings(1, 2, 1));
	HYP_CHECK(!IsValidEnvironmentPrefilterSettings(2048, 512, 1));
	HYP_CHECK(!IsValidEnvironmentPrefilterSettings(2048, 3, 1));
	HYP_CHECK(!IsValidEnvironmentPrefilterSettings(2048, 1, 0));
	HYP_CHECK(!IsValidEnvironmentPrefilterSettings(2048, 1, 1025));
}

void CheckSettingsContract()
{
	const auto& Type = RecordType<FEnvironmentBakeSettings>();
	HYP_CHECK(Type.Id == "asset.import.sky-settings" && Type.Version == 1 && Type.Members.size() == 3);
	const FEnvironmentBakeSettings Defaults;
	HYP_CHECK(Defaults.RadianceSize == 256 && Defaults.SpecularSize == 64 && Defaults.Samples == 256);
	HYP_CHECK(Type.Members[0].Id == "radianceSize" &&
	          Type.Members[0].Options.Description == "Cube face size; power of two from 1 to 1024. Default 256.");
	HYP_CHECK(Type.Members[1].Id == "specularSize" &&
	          Type.Members[1].Options.Description ==
	              "Prefiltered face size; power of two from 1 to 256, no larger than radianceSize. Default 64.");
	HYP_CHECK(Type.Members[2].Id == "samples" &&
	          Type.Members[2].Options.Description == "GGX samples from 1 to 1024. Default 256.");
	const FAssetConversionSettings Settings{.Sky = Defaults};
	for (const auto Extension : {".png", ".gltf"})
	{
		HYP_CHECK(InvalidMessage(
		              [&]
		              {
			              ValidateImportSettings(Settings, Extension, {});
		              }) == "Sky bake settings apply only to HDR/EXR panoramas");
	}
	HYP_CHECK(InvalidMessage(
	              [&]
	              {
		              ValidateImportSettings(Settings, ".hdr", RecordType<FTextureAsset>().Id);
	              }) == "Sky bake settings apply only to HDR/EXR panoramas");
	const std::array<float, 8> Pixels{1, 1, 1, 1, 1, 1, 1, 1};
	HYP_CHECK(InvalidMessage(
	              [&]
	              {
		              (void)BakeEnvironment(Pixels, 1, 2, {1, 1, 1});
	              }) == "Sky requires a 2:1 HDR panorama and bounded power-of-two bake sizes");
	HYP_CHECK(InvalidMessage(
	              [&]
	              {
		              (void)BakeEnvironment({}, 2, 1, {1, 1, 1});
	              }) == "Sky requires a 2:1 HDR panorama and bounded power-of-two bake sizes");
}

void CheckTextureStorage()
{
	for (const auto Format : {ETextureFormat::Rgba16Float, ETextureFormat::Rgba32Float})
	{
		FMaterialTextureMip Mip{1, 1, std::vector<std::uint8_t>(TexturePixelBytes(Format))};
		WriteTexturePixel(Mip, Format, 0, {65504, .00001f, -1, 1});
		const auto Pixel = ReadTexturePixel(Mip, Format, 0);
		HYP_CHECK(Pixel[0] == 65504 && Pixel[2] == -1 && std::abs(Pixel[1] - .00001f) < .00000003f);
	}
	auto Texture = BuildTextureAsset("Legacy", EMaterialTextureEncoding::Srgb, {1, 1, {20, 30, 40, 255}});
	auto Node = WriteRecord(RecordType<FTextureAsset>(), &Texture);
	auto& Object = std::get<FArchiveNode::FObject>(Node.Value);
	Object.at("version") = WriteValue(std::uint32_t(1));
	auto& Fields = std::get<FArchiveNode::FObject>(Object.at("fields").Value);
	Fields.erase("dimension");
	Fields.erase("format");
	const auto Legacy = ReadValue<FTextureAsset>(Node);
	HYP_CHECK(Legacy.Dimension == ETextureDimension::Texture2D && Legacy.Format == ETextureFormat::Rgba8Unorm);
	Texture.Dimension = ETextureDimension::Cube;
	Rejects(
	    [&]
	    {
		    ValidateTextureAsset(Texture);
	    });
}

void CheckCanonicalImport(FAssetImportService& InImports, FIOService& InIO, const std::filesystem::path& InSource,
                          const std::filesystem::path& InOutput)
{
	auto& Tasks = InIO.TaskSystem();
	FAssetImportOptions Options;
	Options.RootId = DefaultSkyReference().Id;
	Options.Conversion.Sky = FEnvironmentBakeSettings{8, 4, 16};
	Options.Library = InOutput.parent_path();
	const auto First = InImports.ImportAsync(InSource, InOutput, Options).Get(Tasks);
	HYP_CHECK(First->Header.Id == Options.RootId);
	FAssetService Assets(InIO);
	RegisterSceneAssetTypes(Assets.Types());
	auto Reference = DefaultSkyReference();
	Reference.Path = PathToUtf8(InOutput);
	HYP_CHECK(Assets.LoadReferenceAsync(Reference, InOutput).Get(Tasks)->Header.Id == Options.RootId);
	HYP_CHECK(InImports.ImportAsync(InSource, InOutput, Options).Get(Tasks)->bUpToDate);
	const auto Before = InIO.ReadAsync(InOutput).Get(Tasks);
	Rejects(
	    [&]
	    {
		    InImports.ImportAsync(InSource, InOutput.parent_path() / "Duplicate.hasset", Options).Get(Tasks);
	    });
	HYP_CHECK(!InIO.FileSystem()->Exists(InOutput.parent_path() / "Duplicate.hasset"));
	const auto CanonicalId = Options.RootId;
	for (const auto& InvalidId : {std::string("invalid"), CreateIdentifier()})
	{
		Options.RootId = InvalidId;
		Rejects(
		    [&]
		    {
			    InImports.ImportAsync(InSource, InOutput, Options).Get(Tasks);
		    });
		HYP_CHECK(*InIO.ReadAsync(InOutput).Get(Tasks) == *Before);
	}
	Options.RootId = CanonicalId;
	InIO.WriteAsync(InOutput, {std::byte{0}}).Get(Tasks);
	const auto Rebuilt = InImports.ImportAsync(InSource, InOutput, Options).Get(Tasks);
	HYP_CHECK(Rebuilt->Header.Id == CanonicalId);
	Assets.ClearCache();
	HYP_CHECK(Assets.LoadReferenceAsync(Reference, InOutput).Get(Tasks)->Header.Id == CanonicalId);
	HYP_CHECK(Assets.LoadGraphAsync(InOutput).Get(Tasks)->Failures.empty());
}

void CheckImports()
{
	FTaskSystem Tasks{2, 1};
	FIOService IO(Tasks);
	FAssetImportService Imports(IO);
	RegisterGltfImporter(Imports);
	RegisterSkyImporter(Imports);
	const auto Root = std::filesystem::path(HYP_SOURCE_DIR);
	for (const auto* Name : {"Cloudy.hdr", "Dusk.exr", "Clear.hdr"})
	{
		const auto Bytes =
		    IO.ReadAsync(std::filesystem::path(HYP_TEST_OUTPUT_DIR) / "fixtures/Sources/Skies" / Name).Get(Tasks);
		const auto Image = DecodeHdrImage(*Bytes);
		HYP_CHECK(Image.Width == Image.Height * 2 && Image.Width == 1024);
		HYP_CHECK(*std::max_element(Image.Rgba.begin(), Image.Rgba.end()) > 1);
	}
	const auto Work = std::filesystem::absolute("environment-test") / CreateIdentifier();
	const auto Output = Work / "Cloudy.hasset";
	const auto Source = std::filesystem::path(HYP_TEST_OUTPUT_DIR) / "fixtures/Sources/Skies/Cloudy.hdr";
	FAssetImportOptions Options;
	Options.Conversion.Sky = FEnvironmentBakeSettings{8, 4, 16};
	const auto Imported = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
	HYP_CHECK(Imported->WrittenAssets >= 4 || Imported->bUpToDate);
	FAssetService Assets(IO);
	RegisterSceneAssetTypes(Assets.Types());
	const auto Graph = Assets.LoadGraphAsync(Output).Get(Tasks);
	HYP_CHECK(Graph->Failures.empty() && Graph->Root->As<FSkyAsset>()->Convention == 1);
	HYP_CHECK(Graph->Assets.size() == 4);
	HYP_CHECK(Imports.ImportAsync(Source, Output, Options).Get(Tasks)->bUpToDate);
	CheckCanonicalImport(Imports, IO, Source, Work / "Canonical" / "Cloudy.hasset");
	std::cout << "HDR/EXR decode, native dependency graph and incremental import passed\n";
}
} // namespace

int main()
{
	try
	{
		CheckNumerics();
		CheckBakeSettings();
		CheckSettingsContract();
		CheckTextureStorage();
		CheckImports();
		std::cout << "Environment preprocessing passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
