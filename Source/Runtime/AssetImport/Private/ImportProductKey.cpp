#include "ImportProductKey.h"

namespace Hyperion::Private
{
namespace
{
constexpr std::string_view ImagePrefix = "image/";

struct FImageEncodingToken
{
	EMaterialTextureEncoding Encoding;
	std::string_view Delimiter;
};

// Legacy parsing prefers the last sRGB delimiter, even when a Linear delimiter occurs later in the key.
constexpr FImageEncodingToken ImageEncodings[] = {{EMaterialTextureEncoding::Srgb, "/srgb/"},
                                                  {EMaterialTextureEncoding::Linear, "/linear/"}};
} // namespace

std::string FormatSharedImageKey(const FSharedImageKey& InKey)
{
	for (const auto& Entry : ImageEncodings)
	{
		if (Entry.Encoding == InKey.Encoding)
		{
			return std::string(ImagePrefix) + InKey.Source + std::string(Entry.Delimiter) + InKey.Recipe;
		}
	}
	throw std::invalid_argument("Invalid shared image key encoding");
}

std::optional<FSharedImageKey> ParseSharedImageKey(std::string_view InKey)
{
	if (InKey.starts_with(ImagePrefix))
	{
		for (const auto& Entry : ImageEncodings)
		{
			const auto End = InKey.rfind(Entry.Delimiter);
			if (End != std::string_view::npos)
			{
				// Keep the old substring range even when a custom key overlaps the prefix's trailing slash.
				return FSharedImageKey{std::string(InKey.substr(ImagePrefix.size(), End - ImagePrefix.size())),
				                       Entry.Encoding, std::string(InKey.substr(End + Entry.Delimiter.size()))};
			}
		}
	}
	return {};
}

std::string FormatImportProductKey(FSourceProductKey InKey)
{
	return "source/" + std::string(InKey.Source) + "|" + std::string(InKey.TypeId);
}

std::string FormatImportProductKey(FNamedProductKey InKey)
{
	return "product/" + std::string(InKey.Source) + "/" + std::string(InKey.Product) + "|" + std::string(InKey.TypeId);
}

std::string FormatImportProductKey(FSharedProductKey InKey)
{
	return "shared/" + std::string(InKey.Shared) + "|" + std::string(InKey.TypeId);
}
} // namespace Hyperion::Private
