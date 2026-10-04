#include "AssetPublicationInternal.h"
#include "Hyperion/IO/Path.h"
#include "ImportProductKey.h"
#include <array>

namespace Hyperion::Private
{
namespace
{
void Require(bool bInCondition, const char* InMessage)
{
	if (!bInCondition)
	{
		throw std::runtime_error(InMessage);
	}
}

void CheckImageKeyContract()
{
	struct FImageCase
	{
		std::string_view Text;
		std::string_view Source;
		EMaterialTextureEncoding Encoding;
		std::string_view Recipe;
	};

	const std::array<FImageCase, 6> Cases{{
	    {"image/fixture/Texture.png/srgb/rgba8-full-mips-v1", "fixture/Texture.png", EMaterialTextureEncoding::Srgb,
	     "rgba8-full-mips-v1"},
	    {"image/fixture/Texture.png/linear/rgba8-full-mips-v1", "fixture/Texture.png", EMaterialTextureEncoding::Linear,
	     "rgba8-full-mips-v1"},
	    {"image/a/srgb/b/linear/recipe", "a", EMaterialTextureEncoding::Srgb, "b/linear/recipe"},
	    {"image/a/srgb/b/srgb/recipe", "a/srgb/b", EMaterialTextureEncoding::Srgb, "recipe"},
	    {"image/a/linear/future|type", "a", EMaterialTextureEncoding::Linear, "future|type"},
	    {"image//srgb/", "", EMaterialTextureEncoding::Srgb, ""},
	}};
	for (const auto& Entry : Cases)
	{
		const auto Parsed = ParseSharedImageKey(Entry.Text);
		Require(Parsed && Parsed->Source == Entry.Source && Parsed->Encoding == Entry.Encoding &&
		            Parsed->Recipe == Entry.Recipe,
		        "Legacy image key parse changed");
		Require(FormatSharedImageKey(*Parsed) == Entry.Text, "Legacy image key bytes changed");
	}
	Require(FormatSharedImageKey({"fixture/T.png", EMaterialTextureEncoding::Srgb}) ==
	            "image/fixture/T.png/srgb/rgba8-full-mips-v1",
	        "Default image recipe changed");
	const auto Overlap = ParseSharedImageKey("image/srgb/r");
	Require(Overlap && Overlap->Source == "srgb/r" && Overlap->Recipe == "r", "Legacy overlapping range changed");
	for (const auto* Text : {"", "vendor/custom|identity/v2", "builtin/white-rgba8-v1", "image/a/unknown/r",
	                         "shared/image/a/srgb/r", "Image/a/srgb/r", "image/a/SRGB/r"})
	{
		Require(!ParseSharedImageKey(Text), "Opaque shared key was interpreted as an image");
	}
	bool bRejected{};
	try
	{
		(void)FormatSharedImageKey({"a", static_cast<EMaterialTextureEncoding>(255)});
	}
	catch (const std::invalid_argument&)
	{
		bRejected = true;
	}
	Require(bRejected, "Invalid native image encoding was accepted");
}

void CheckPublicationKeyContract()
{
	Require(FormatImportProductKey(FSourceProductKey{"fixture/Child.identity", "test.import-key"}) ==
	            "source/fixture/Child.identity|test.import-key",
	        "Source product key bytes changed");
	Require(FormatImportProductKey(FNamedProductKey{"fixture/Root.identity", "named-part", "test.import-key"}) ==
	            "product/fixture/Root.identity/named-part|test.import-key",
	        "Named product key bytes changed");
	Require(FormatImportProductKey(FSharedProductKey{"vendor/custom|identity/v2", "test.import-key"}) ==
	            "shared/vendor/custom|identity/v2|test.import-key",
	        "Opaque shared publication key bytes changed");
	Require(std::string_view(RootImportProductKey) == "$root" &&
	            std::string_view(TextureContentSetting) == "texture_content" &&
	            std::string_view(BuiltinWhiteSharedKey) == "builtin/white-rgba8-v1",
	        "Persisted import constants changed");
}

void CheckPortableKeys()
{
	FTaskSystem Tasks{1, 1};
	FIOService IO{Tasks, std::make_shared<FMemoryFileSystem>()};
	const std::vector<FAssetImporter> Importers;
	FPublication Publication{IO, {}, Importers};
	Publication.SourceRoot = std::filesystem::absolute("portable-key").lexically_normal();
	Publication.SourceId = "fixture";
	const auto Root = PathToUtf8(Publication.SourceRoot);

	struct FKeyCase
	{
		std::string Input;
		std::string Expected;
	};

	const std::array<FKeyCase, 12> Cases{{
	    {"image/" + Root + "/Texture.png/srgb/rgba8-full-mips-v1", "image/fixture/Texture.png/srgb/rgba8-full-mips-v1"},
	    {"image/" + Root + "/Texture.png/linear/future/recipe|type",
	     "image/fixture/Texture.png/linear/future/recipe|type"},
	    {"image/" + Root + "/Texture.png/srgb/", "image/fixture/Texture.png/srgb/"},
	    {"image/" + Root + "/a/srgb/b/linear/recipe", "image/fixture/a/srgb/b/linear/recipe"},
	    {"image/" + Root + "/a/srgb/b/srgb/recipe", "image/fixture/a/srgb/b/srgb/recipe"},
	    {"vendor/" + Root + "/x|" + Root + "/y", "vendor/fixture/x|fixture/y"},
	    {"shared/image/" + Root + "/Texture.png/srgb/r|type", "shared/image/fixture/Texture.png/srgb/r|type"},
	    {"product/" + Root + "/Root/name|type", "product/fixture/Root/name|type"},
	    {"vendor/custom|identity/v2", "vendor/custom|identity/v2"},
	    {"builtin/white-rgba8-v1", "builtin/white-rgba8-v1"},
	    {"image/unknown/form", "image/unknown/form"},
	    {"$root", "$root"},
	}};
	for (const auto& Entry : Cases)
	{
		Require(Publication.PortableKey(Entry.Input) == Entry.Expected, "Legacy portable key bytes changed");
	}
	for (const auto* Key : {"custom/D:/unmapped/file", "custom\\local", "image/fixture/X/srgb/r", "image/srgb/r"})
	{
		bool bRejected{};
		try
		{
			(void)Publication.PortableKey(Key);
		}
		catch (const std::runtime_error&)
		{
			bRejected = true;
		}
		Require(bRejected, "Legacy unportable key rejection changed");
	}
	const std::string EmbeddedNull("vendor/a\0b", 10);
	Require(Publication.PortableKey(EmbeddedNull) == EmbeddedNull, "Opaque key bytes were truncated");
}
} // namespace

void CheckImportProductKeys()
{
	CheckImageKeyContract();
	CheckPublicationKeyContract();
	CheckPortableKeys();
}
} // namespace Hyperion::Private
