#include "Hyperion/AssetImport/AssetImportService.h"
#include "Hyperion/IO/Path.h"
#include "Support/TestSupport.h"
#include <array>

namespace Hyperion
{
struct FImportKeyFixture
{
	std::string Name;
	std::vector<FAssetRef> Children;
};

template<> const FRecordDescriptor& RecordType<FImportKeyFixture>()
{
	static const auto Type =
	    MakeRecord<FImportKeyFixture>("test.import-key", {Member("name", &FImportKeyFixture::Name),
	                                                      Member("children", &FImportKeyFixture::Children)});
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

struct FLegacyProduct
{
	std::string Key;
	std::string Id;
	std::string Filename;
};

const std::array<FLegacyProduct, 7> LegacyProducts{{
    {"$root", "00000000000000000000000000000001", "Root.hasset"},
    {"product/fixture/Root.identity/named-part|test.import-key", "00000000000000000000000000000002", "Named.hasset"},
    {"shared/vendor/custom|identity/v2|test.import-key", "00000000000000000000000000000003", "Opaque.hasset"},
    {"shared/image/fixture/Texture.png/srgb/rgba8-full-mips-v1|test.import-key", "00000000000000000000000000000004",
     "Srgb.hasset"},
    {"shared/image/fixture/Texture.png/linear/rgba8-full-mips-v1|test.import-key", "00000000000000000000000000000005",
     "Linear.hasset"},
    {"shared/builtin/white-rgba8-v1|test.import-key", "00000000000000000000000000000006", "White.hasset"},
    {"source/fixture/Child.identity|test.import-key", "00000000000000000000000000000007", "Child.hasset"},
}};

std::map<std::string, std::string> LegacyOutputIds()
{
	std::map<std::string, std::string> Result;
	for (const auto& Product : LegacyProducts)
	{
		Result.emplace(Product.Key, Product.Id);
	}
	return Result;
}

FAssetImporter KeyImporter()
{
	return {"test.key-importer",
	        1,
	        &RecordType<FImportKeyFixture>(),
	        {".identity"},
	        [](FAssetImportContext& InContext)
	        {
		        auto Result = std::make_shared<FImportKeyFixture>();
		        Result->Name = PathToUtf8(InContext.Path.filename());
		        if (InContext.Path.filename() == "Child.identity")
		        {
			        return Result;
		        }
		        const auto Image = "image/" + PathToUtf8(InContext.Path.parent_path() / "Texture.png");
		        Result->Children = {
		            InContext.Emit("named-part", FImportKeyFixture{"Named"}),
		            InContext.Emit("opaque", FImportKeyFixture{"Opaque"}, "vendor/custom|identity/v2"),
		            InContext.Emit("srgb", FImportKeyFixture{"Srgb"}, Image + "/srgb/rgba8-full-mips-v1"),
		            InContext.Emit("linear", FImportKeyFixture{"Linear"}, Image + "/linear/rgba8-full-mips-v1"),
		            InContext.Emit("white", FImportKeyFixture{"White"}, "builtin/white-rgba8-v1"),
		            {"", "Child.identity", "test.import-key", ""}};
		        return Result;
	        }};
}

void SeedLegacyAssets(FIOService& InIO, const std::filesystem::path& InLibrary,
                      const std::filesystem::path& InSourceRoot, bool bInPhysicalKeys)
{
	for (const auto& Product : LegacyProducts)
	{
		FAssetHeader Header;
		Header.Id = Product.Id;
		if (Product.Key == "$root")
		{
			Header.Import.emplace();
			for (const auto& [Key, Id] : LegacyOutputIds())
			{
				auto OldKey = Key;
				if (bInPhysicalKeys)
				{
					const auto Position = OldKey.find("fixture/");
					if (Position != std::string::npos)
					{
						OldKey.replace(Position, std::string_view("fixture/").size(), PathToUtf8(InSourceRoot) + "/");
					}
				}
				Header.Import->OutputIds.emplace(std::move(OldKey), Id);
			}
		}
		// The old root's mappings also enter the library index without normalization; these are non-texture products.
		// Physical source keys exercise PreviousIds normalization, while logical/opaque keys allow library fallback.
		const FImportKeyFixture Object{"Legacy"};
		const auto Encoded = EncodeAsset(RecordType<FImportKeyFixture>(), &Object, std::move(Header));
		InIO.WriteAsync(InLibrary / Product.Filename, Encoded.Bytes).Get(InIO.TaskSystem());
	}
}

void CheckHistoricalPublication(bool bInPhysicalKeys)
{
	FTaskSystem Tasks{1, 1};
	FIOService IO{Tasks, std::make_shared<FMemoryFileSystem>()};
	FAssetImportService Imports{IO};
	Imports.Register(KeyImporter());
	const auto Directory = std::filesystem::absolute("legacy-import-key").lexically_normal();
	const auto Source = Directory / "Root.identity";
	FAssetImportOptions Options;
	Options.Library = Directory / "native";
	Options.SourceRoot = Directory;
	Options.SourceId = "fixture";
	IO.WriteAsync(Source, {}).Get(Tasks);
	IO.WriteAsync(Directory / "Child.identity", {}).Get(Tasks);
	SeedLegacyAssets(IO, Options.Library, Directory, bInPhysicalKeys);
	const auto Output = Options.Library / "Root.hasset";
	const auto First = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
	HYP_CHECK(First->Header.Id == LegacyProducts.front().Id && First->WrittenAssets == 7);
	HYP_CHECK(First->Header.Import->OutputIds == LegacyOutputIds());
	for (const auto& Product : LegacyProducts)
	{
		const auto Document = DecodeAsset(IO.ReadAsync(Options.Library / Product.Filename).Get(Tasks));
		HYP_CHECK(Document.Header.Id == Product.Id);
		HYP_CHECK(Document.Header.Import->OutputIds.at(Product.Key) == Product.Id);
	}
	const auto Before = IO.ReadAsync(Output).Get(Tasks);
	Options.bForce = true;
	const auto Again = Imports.ImportAsync(Source, Output, Options).Get(Tasks);
	HYP_CHECK(Again->Header.Import->OutputIds == LegacyOutputIds());
	HYP_CHECK(*Before == *IO.ReadAsync(Output).Get(Tasks));
	IO.WriteAsync(Directory / "Other.identity", {}).Get(Tasks);
	const auto Other =
	    Imports.ImportAsync(Directory / "Other.identity", Options.Library / "Other.hasset", Options).Get(Tasks);
	HYP_CHECK(Other->Header.Id != First->Header.Id);
	for (const auto& Product : LegacyProducts)
	{
		if (Product.Key.starts_with("shared/") || Product.Key.starts_with("source/"))
		{
			HYP_CHECK(Other->Header.Import->OutputIds.at(Product.Key) == Product.Id);
		}
	}
	HYP_CHECK(Other->Header.Import->OutputIds.at("product/fixture/Other.identity/named-part|test.import-key") !=
	          LegacyProducts[1].Id);
}
} // namespace

void CheckImportProductIdentity()
{
	CheckHistoricalPublication(false);
	CheckHistoricalPublication(true);
}
