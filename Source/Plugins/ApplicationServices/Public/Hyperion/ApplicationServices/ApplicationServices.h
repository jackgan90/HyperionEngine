#pragma once
#include "Hyperion/Assets/AssetService.h"
#include "Hyperion/IO/MountedFileSystem.h"
#include "Hyperion/Plugins/PluginRuntime.h"
#include "Hyperion/RHI/RHIBackend.h"
#include <optional>

namespace Hyperion
{
void RegisterContactShadowServices(FPluginRegistry& InRegistry);
using FRegisterBackends = std::function<void(FRHIBackendRegistry&)>;

struct FAssetServiceOptions
{
	std::filesystem::path Mounts;
	bool bRequireCatalogs{};
};

struct FWindowServiceOptions
{
	std::string Title;
	FSize Size{1280, 720};
	bool bHidden{};
	bool bDarkTitleBar{};
};

struct FGraphicsServiceOptions
{
	FRegisterBackends RegisterBackends;
	std::string Backend = "d3d12";
	std::filesystem::path ShaderCache;
	bool bReversedZ = true;
	std::uint32_t TimingFrames{};
};

struct FGuiServiceOptions
{
	bool bEditorStyle{};
	std::string Font;
	float FontSize = 15;
	std::filesystem::path Layout;
	std::filesystem::path Preferences;
	std::optional<float> ApplicationScale;
};

void RegisterAssetServices(FPluginRegistry& InRegistry, FAssetServiceOptions InOptions);
void RegisterWindowServices(FPluginRegistry& InRegistry, FWindowServiceOptions InOptions);
void RegisterGraphicsServices(FPluginRegistry& InRegistry, FGraphicsServiceOptions InOptions);
void RegisterGuiServices(FPluginRegistry& InRegistry, FGuiServiceOptions InOptions = {});
} // namespace Hyperion
