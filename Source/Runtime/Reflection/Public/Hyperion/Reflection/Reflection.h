#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Hyperion
{
using FValue = std::variant<bool, std::int64_t, double, std::string, std::vector<std::string>>;
enum class EPropertyKind
{
	Boolean,
	Integer,
	Number,
	String,
	StringList
};

struct FProperty
{
	std::string Id;
	std::string Label;
	EPropertyKind Kind;
	double Minimum{};
	double Maximum{};
	bool bPersistent = true;
	std::function<FValue(const void*)> Get;
	std::function<void(void*, const FValue&)> Set;
};

struct FTypeDescriptor
{
	std::string Id;
	std::uint32_t Version;
	std::vector<FProperty> Properties;
};

void SaveReflected(const std::filesystem::path& InPath, const FTypeDescriptor& InType, const void* InObject);
std::string EncodeReflected(const FTypeDescriptor& InType, const void* InObject);
void DecodeReflected(std::string_view InText, const FTypeDescriptor& InType, void* InObject);
void LoadReflected(const std::filesystem::path& InPath, const FTypeDescriptor& InType, void* InObject);
} // namespace Hyperion
