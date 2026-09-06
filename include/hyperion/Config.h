#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
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
	bool Persistent = true;
	std::function<FValue(const void*)> Get;
	std::function<void(void*, const FValue&)> Set;
};

struct FTypeDescriptor
{
	std::string Id;
	std::uint32_t Version;
	std::vector<FProperty> Properties;
};

struct FAppSettings
{
	std::string Title = "Hyperion | Rendering Lab";
	int Width = 1280;
	int Height = 720;
	int Workers = 4;
	int RhiThreads = 2;
	bool Vsync = true;
	bool ShowGui = true;
	double TriangleScale = 0.85;
	double ClearRed = 0.025;
	double ClearGreen = 0.035;
	double ClearBlue = 0.065;
	std::vector<std::string> Plugins{"triangle", "debug-ui"};
};

const FTypeDescriptor& SettingsType();
void SaveReflected(const std::filesystem::path& InPath, const FTypeDescriptor& InType, const void* InObject);
void LoadReflected(const std::filesystem::path& InPath, const FTypeDescriptor& InType, void* InObject);
void SaveSettings(const std::filesystem::path& InPath, const FAppSettings& InSettings);
FAppSettings LoadSettings(const std::filesystem::path& InPath);
} // namespace Hyperion
