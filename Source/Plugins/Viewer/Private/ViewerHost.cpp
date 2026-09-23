#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Viewer/ViewerPlugin.h"
#include "ViewerApplication.h"
#if HYP_ENABLE_TRIANGLE
#include "Hyperion/Triangle/TrianglePlugin.h"
#endif
#if HYP_ENABLE_MODEL_VIEWER
#include "Hyperion/ModelViewer/ModelViewerPlugin.h"
#endif
#if HYP_ENABLE_SCENE_VIEWER
#include "Hyperion/SceneViewer/SceneViewerPlugin.h"
#endif
#if HYP_ENABLE_DEBUG_UI
#include "Hyperion/DebugUI/DebugUIPlugin.h"
#endif
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/RenderDoc/RenderDocPlugin.h"
#endif
#include <algorithm>

namespace Hyperion
{
namespace
{
void RegisterViewerCatalog(FPluginRegistry& InRegistry, const FAppSettings& InSettings)
{
#if HYP_ENABLE_TRIANGLE
	RegisterTrianglePlugin(InRegistry);
#endif
#if HYP_ENABLE_MODEL_VIEWER
	RegisterModelViewerPlugin(InRegistry, InSettings.ModelSource);
#endif
#if HYP_ENABLE_SCENE_VIEWER
	RegisterSceneViewerPlugin(InRegistry, InSettings.SceneSource);
#endif
#if HYP_ENABLE_DEBUG_UI
	RegisterDebugUiPlugin(InRegistry);
#endif
#if HYP_ENABLE_RENDERDOC
	RegisterRenderDocPlugin(
	    InRegistry,
	    {std::filesystem::path(std::u8string(InSettings.RenderDocLibrary.begin(), InSettings.RenderDocLibrary.end())),
	     std::filesystem::path(std::u8string(InSettings.RenderDocOutput.begin(), InSettings.RenderDocOutput.end())),
	     InSettings.ModelSource.empty() ? "Triangle" : "Model"});
#endif
	(void)InSettings;
}

void RegisterViewer(FPluginRegistry& InRegistry, const FOptions& InOptions, const FAppSettings& InSettings)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "viewer";
	Descriptor.Dependencies = {"graphics"};
	Descriptor.Requires = {typeid(FApplicationControl),   typeid(FTaskSystem),     typeid(FIOService),
	                       typeid(FAssetService),         typeid(FWindow),         typeid(IRHIDevice),
	                       typeid(IRHISwapchain),         typeid(FShaderCompiler), typeid(FRenderSession),
	                       typeid(FRenderFeatureRegistry)};
	Descriptor.Optional = {typeid(IScenePlugin), typeid(ISceneEditor), typeid(FGui), typeid(FGuiRenderer)};
#if HYP_ENABLE_RENDERDOC
	Descriptor.Optional.push_back(typeid(FFrameCapture));
#endif
	Descriptor.After = {"debug-ui", "contact-shadows"};
	Descriptor.Create = [InOptions, InSettings]
	{
		return std::make_unique<FViewerPlugin>(InOptions, InSettings);
	};
	InRegistry.Add(std::move(Descriptor));
}

void RegisterViewerServices(FPluginRegistry& InRegistry, const FOptions& InOptions, const FAppSettings& InSettings,
                            FRegisterBackends InBackends)
{
	RegisterAssetServices(InRegistry, {InOptions.EngineContent, InOptions.AssetRoot, InOptions.bReadOnly});
	RegisterWindowServices(InRegistry,
	                       {InSettings.Title,
	                        {static_cast<unsigned>(InSettings.Width), static_cast<unsigned>(InSettings.Height)},
	                        InOptions.bHidden});
	RegisterGraphicsServices(InRegistry,
	                         {std::move(InBackends), InSettings.RHIBackend,
	                          std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache", InSettings.bReversedZ,
	                          InOptions.Benchmark.empty()
	                              ? 0u
	                              : static_cast<std::uint32_t>(InOptions.Frames > 0 ? InOptions.Frames : 65536)});
	FGuiServiceOptions GuiOptions;
	if (InOptions.Benchmark.empty() && !InOptions.bHidden)
	{
		GuiOptions.Preferences = std::filesystem::path(HYP_SOURCE_DIR) / "out/viewer/UiScale.ini";
	}
	RegisterGuiServices(InRegistry, std::move(GuiOptions));
	RegisterContactShadowServices(InRegistry);
}
} // namespace

void RunViewerApplication(int InCount, char** InValues, FRegisterBackends InBackends)
{
	auto Options = ParseOptions(InCount, InValues);
	auto Settings = LoadSettings(Options.Config);
	ApplyOptions(Options, Settings);
	FApplicationHost Host(static_cast<unsigned>(Settings.Workers), static_cast<unsigned>(Settings.RhiThreads));
	FPluginRegistry Registry;
	RegisterAutomationServices(Registry);
	RegisterSceneAutomation(Registry);
	RegisterAutomationLocal(Registry, "Viewer");
	RegisterViewerServices(Registry, Options, Settings, std::move(InBackends));
	RegisterViewerCatalog(Registry, Settings);
	RegisterViewer(Registry, Options, Settings);
	FPluginSelection Selection;
	Selection.Disabled = Settings.DisabledPlugins;
	Selection.Disabled.insert(Selection.Disabled.end(), Options.DisabledPlugins.begin(), Options.DisabledPlugins.end());
	if (Options.bNoUi)
	{
		Selection.Disabled.push_back("debug-ui");
		Selection.Disabled.push_back("gui");
	}
	if (!Options.bKernelOnly)
	{
		Selection.Requested = Settings.Plugins;
		if (Options.bVerifyClear)
		{
			std::erase_if(Selection.Requested,
			              [](const auto& InId)
			              {
				              return InId != "renderdoc";
			              });
		}
		Selection.Requested.push_back("contact-shadows");
		Selection.Requested.push_back("viewer");
		Selection.Requested.push_back("automation-scene");
		Selection.Requested.push_back("automation-local");
	}
	Host.Start(Registry, Selection);
	if (!Host.GetPlugins().IsActive("viewer"))
	{
		for (const auto& Diagnostic : Host.GetPlugins().GetDiagnostics())
		{
			if (Diagnostic.bStartupFailure)
			{
				throw std::runtime_error(Diagnostic.Id + ": " + Diagnostic.Message);
			}
		}
		if (!Options.Capture.empty() || !Options.Benchmark.empty() || !Options.SaveConfig.empty() ||
		    !Options.SaveScene.empty() || !Options.RdcFrames.empty())
		{
			throw std::runtime_error("Requested Viewer output is unavailable: viewer plugin did not start");
		}
	}
	const bool bViewerActive = Host.GetPlugins().IsActive("viewer");
	Host.Run(bViewerActive ? 0 : static_cast<unsigned>(std::max(1, Options.Frames)));
	Host.Stop();
	Host.GetServices().Require<FApplicationControl>().RethrowFailure();
	if (MemoryStats(EMemoryTag::Gui).LiveBytes || MemoryStats(EMemoryTag::Render).LiveBytes)
	{
		throw std::runtime_error("GUI or renderer hooked allocations survived shutdown");
	}
	if (bViewerActive)
	{
		Log(ELogLevel::Info, "Rendering lifecycle completed successfully");
	}
}
} // namespace Hyperion
