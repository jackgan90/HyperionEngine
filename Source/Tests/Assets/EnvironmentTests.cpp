#include "Hyperion/AssetImport/GltfImport.h"
#include "Hyperion/AssetImport/SceneImport.h"
#include "Hyperion/AssetImport/SkyImport.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Core/ContentHash.h"
#include "Hyperion/IO/Path.h"
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

void CheckImports()
{
	FTaskSystem Tasks{2, 1};
	FIOService IO(Tasks);
	FAssetImportService Imports(IO);
	RegisterGltfImporter(Imports);
	RegisterSceneImporter(Imports);
	const auto Root = std::filesystem::path(HYP_SOURCE_DIR);
	for (const auto* Name : {"Cloudy.hdr", "Dusk.exr", "Clear.hdr"})
	{
		const auto Bytes = IO.ReadAsync(Root / "out/fixtures/Sources/Skies" / Name).Get(Tasks);
		const auto Image = DecodeHdrImage(*Bytes);
		HYP_CHECK(Image.Width == Image.Height * 2 && Image.Width == 1024);
		HYP_CHECK(*std::max_element(Image.Rgba.begin(), Image.Rgba.end()) > 1);
	}
	const auto Work = std::filesystem::absolute("environment-test") / CreateIdentifier();
	const auto Output = Work / "Cloudy.hasset";
	const std::string Descriptor = "{\"type\":\"hyperion.skyasset\",\"schema_version\":1,\"source\":\"" +
	                               PathToUtf8(Root / "out/fixtures/Sources/Skies/Cloudy.hdr") +
	                               "\",\"radiance_size\":8,\"specular_size\":4,\"samples\":16}";
	const auto Bytes = std::as_bytes(std::span(Descriptor));
	IO.WriteAsync(Work / "Cloudy.json", FBytes(Bytes.begin(), Bytes.end())).Get(Tasks);
	const auto Imported = Imports.ImportAsync(Work / "Cloudy.json", Output).Get(Tasks);
	HYP_CHECK(Imported->WrittenAssets >= 4 || Imported->bUpToDate);
	FAssetService Assets(IO);
	RegisterSceneAssetTypes(Assets.Types());
	const auto Graph = Assets.LoadGraphAsync(Output).Get(Tasks);
	HYP_CHECK(Graph->Failures.empty() && Graph->Root->As<FSkyAsset>()->Convention == 1);
	HYP_CHECK(Graph->Assets.size() == 4);
	HYP_CHECK(Imports.ImportAsync(Work / "Cloudy.json", Output).Get(Tasks)->bUpToDate);
	std::cout << "HDR/EXR decode, native dependency graph and incremental import passed\n";
}
} // namespace

int main()
{
	try
	{
		CheckNumerics();
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
