#pragma once
#include "Hyperion/Textures/TextureAsset.h"
#include <optional>
#include <string_view>

namespace Hyperion::Private
{
inline constexpr char RootImportProductKey[] = "$root";
inline constexpr char TextureContentSetting[] = "texture_content";
inline constexpr char BuiltinWhiteSharedKey[] = "builtin/white-rgba8-v1";
inline constexpr char ImageImportRecipe[] = "rgba8-full-mips-v1";

struct FSharedImageKey
{
	std::string Source;
	EMaterialTextureEncoding Encoding = EMaterialTextureEncoding::Linear;
	std::string Recipe = ImageImportRecipe;
};

std::string FormatSharedImageKey(const FSharedImageKey& InKey);
std::optional<FSharedImageKey> ParseSharedImageKey(std::string_view InKey);

// Borrowed inputs are consumed immediately by the publication formatters; stored keys remain owning strings.
struct FSourceProductKey
{
	std::string_view Source;
	std::string_view TypeId;
};

struct FNamedProductKey
{
	std::string_view Source;
	std::string_view Product;
	std::string_view TypeId;
};

struct FSharedProductKey
{
	std::string_view Shared;
	std::string_view TypeId;
};

std::string FormatImportProductKey(FSourceProductKey InKey);
std::string FormatImportProductKey(FNamedProductKey InKey);
std::string FormatImportProductKey(FSharedProductKey InKey);
} // namespace Hyperion::Private
