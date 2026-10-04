#include "Hyperion/AssetImport/ImageImport.h"
#include "Hyperion/Assets/Assets.h"
#include "Hyperion/Textures/TextureAsset.h"
#include "Support/TestSupport.h"

void CheckImageImportIdentity()
{
	using namespace Hyperion;
	FTaskSystem Tasks{1, 1};
	FIOService IO{Tasks, std::make_shared<FMemoryFileSystem>()};
	FAssetImportService Imports{IO};
	FAssetService Assets{IO};
	Assets.Types().Register(RecordType<FTextureAsset>());
	RegisterImageImporter(Imports);
	const auto Root = std::filesystem::absolute("image-identity");
	const auto Source = Root / "Source.png";
	IO.WriteAsync(Source, EncodePng(FImage{1, 1, EColorSpace::Srgb, {1, 0, 0, 1}})).Get(Tasks);
	for (const bool bPrepared : {false, true})
	{
		FAssetImportOptions Options;
		Options.Name = bPrepared ? "Prepared texture" : "Direct texture";
		const auto Output = Root / (bPrepared ? "Prepared.hasset" : "Direct.hasset");
		if (bPrepared)
		{
			Options.Prepared = Imports.PrepareAsync(Source, Output, Options).Get(Tasks);
			HYP_CHECK(Options.Prepared->Root.Importer == "hyperion.image");
			HYP_CHECK(Options.Prepared->Root.ImporterVersion == 1);
			HYP_CHECK(Options.Prepared->Root.Type->Id == "hyperion.textureasset");
		}
		const auto Result = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
		HYP_CHECK(!Result->bUpToDate && Result->WrittenAssets == 1);
		const auto Loaded = Assets.LoadAsync<FTextureAsset>(Output).GetAsset(Tasks);
		HYP_CHECK(Loaded->Header.TypeId == "hyperion.textureasset");
		HYP_CHECK(Loaded->Header.Import.has_value());
		HYP_CHECK(Loaded->Header.Import->Importer == "hyperion.image");
		HYP_CHECK(Loaded->Header.Import->ImporterVersion == 1);
		HYP_CHECK(Loaded->As<FTextureAsset>()->Name == Options.Name);
	}
}
