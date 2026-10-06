#include "EditorApplication.h"
#include "EditorHostOptions.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Editor/EditorPlugin.h"
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/RenderDoc/RenderDocPlugin.h"
#endif

namespace Hyperion
{
namespace
{
FEditorOptions LoadStartupOptions(int InCount, char** InValues, FLogHistory* InLogHistory,
                                  std::shared_ptr<FStorageSettings> InStorage)
{
	auto Options = ParseEditorOptions(InCount, InValues, std::move(InStorage));
	Options.Storage->ProtectDirectory(Options.EngineContent);
	if (Options.AssetRoot)
	{
		Options.Storage->ProtectDirectory(*Options.AssetRoot);
	}
	Options.LogHistory = InLogHistory;
	const auto Defaults = InstalledConfigDirectory() / "Editor";
	if (!Options.RenderSettingsPath.empty())
	{
		Options.Rendering = LoadRenderSettings(Options.RenderSettingsPath);
	}
	try
	{
		const auto PreferenceSource =
		    std::filesystem::exists(Options.PreferencesPath) || Options.Storage->LaunchOptions().bIsolated
		        ? Options.PreferencesPath
		        : Defaults / "Preferences.ini";
		Options.Preferences = LoadEditorPreferences(PreferenceSource);
	}
	catch (const std::exception& Failure)
	{
		Options.PreferenceError = "Could not load editor preferences: " + std::string(Failure.what());
		Log(ELogLevel::Warning, Options.PreferenceError);
	}
	InitializeProfilingSession(Options.Profiling, !Options.Benchmark.empty() && Options.Profiling.Frames != 0);
	const bool bInteractive = !HasEditorAcceptanceRequest(Options) && Options.Benchmark.empty();
	if (Options.RenderSettingsPath.empty() && bInteractive && !Options.bHidden && !Options.Frames)
	{
		Options.RenderSettingsPath = Options.Storage->Paths().Config / "RenderSettings.json";
		Options.Rendering =
		    LoadRenderSettings(std::filesystem::exists(Options.RenderSettingsPath) ? Options.RenderSettingsPath
		                                                                           : Defaults / "RenderSettings.json");
	}
	return Options;
}

void RegisterEditorServices(FPluginRegistry& InRegistry, const FEditorOptions& InOptions, FRegisterBackends InBackends)
{
	RegisterAutomationServices(InRegistry);
	RegisterStorageServices(InRegistry, InOptions.Storage);
	RegisterStorageAutomation(InRegistry);
	RegisterSceneAutomation(InRegistry);
	RegisterLogAutomation(InRegistry);
	RegisterAssetAutomation(InRegistry);
	RegisterAutomationLocal(InRegistry, "Editor", {}, {},
	                        PathFromUtf8(InOptions.Storage->Get().SettingsFile).parent_path() / "Discovery");
#if HYP_ENABLE_RENDERDOC
	RegisterRenderDocPlugin(InRegistry,
	                        {{}, InOptions.Storage->Paths().Captures, "Editor", InOptions.Preferences.bRenderDocHud});
#endif
	const bool bInteractive = !HasEditorAcceptanceRequest(InOptions) && InOptions.Benchmark.empty();
	const auto RestoredRoot = !InOptions.AssetRoot && bInteractive && !InOptions.Preferences.AssetRoot.empty()
	                              ? std::optional(InOptions.Preferences.AssetRoot)
	                              : std::nullopt;
	if (RestoredRoot)
	{
		InOptions.Storage->ProtectDirectory(*RestoredRoot);
	}
	RegisterAssetServices(InRegistry,
	                      {InOptions.EngineContent, InOptions.AssetRoot ? InOptions.AssetRoot : RestoredRoot,
	                       InOptions.bReadOnly, RestoredRoot.has_value()});
	RegisterWindowServices(InRegistry, {"Hyperion Editor", {1600, 960}, InOptions.bHidden, true});
	RegisterGraphicsServices(InRegistry,
	                         {std::move(InBackends), "d3d12", InOptions.Storage->Paths().CacheRoot / "DerivedData",
	                          InOptions.Rendering.bReversedZ});
	const bool bPersistContentLayout = ShouldPersistEditorContentLayout(InOptions);
	RegisterGuiServices(InRegistry, {true, "/Engine/Fonts/RobotoMedium.ttf", 15,
	                                 bPersistContentLayout ? InOptions.Layout : std::filesystem::path{},
	                                 bPersistContentLayout ? InOptions.UiPreferences : std::filesystem::path{},
	                                 InOptions.ApplicationScale});
	RegisterContactShadowServices(InRegistry);
}

void RegisterEditorPlugin(FPluginRegistry& InRegistry, const FEditorOptions& InOptions)
{
	FPluginDescriptor Descriptor;
	Descriptor.Id = "editor";
	Descriptor.Provides = {typeid(FSceneEditDocument), typeid(ISceneDocumentHost),     typeid(IAssetWorkspace),
	                       typeid(IRenderSettings),    typeid(IShadowControls),        typeid(ISceneLightControls),
	                       typeid(IProfilingControl),  typeid(IAssetPreviewWorkspace), typeid(ISceneViewport),
	                       typeid(IScenePlacement),    typeid(IRenderOutput),          typeid(IRenderCaptureControl),
	                       typeid(IRenderDiagnostics), typeid(IApplicationClose)};
	Descriptor.Dependencies = {"gui"};
	if (InOptions.LogHistory)
	{
		Descriptor.Provides.push_back(typeid(FLogHistory));
	}
	Descriptor.After = {"contact-shadows"};
	Descriptor.Optional = {typeid(FAssetImportWorkspace), typeid(FStorageSettings)};
#if HYP_ENABLE_RENDERDOC
	Descriptor.Optional.push_back(typeid(FFrameCapture));
#endif
	Descriptor.Requires = {typeid(FApplicationControl),
	                       typeid(FContentRootService),
	                       typeid(FTaskSystem),
	                       typeid(FMountedFileSystem),
	                       typeid(FIOService),
	                       typeid(FAssetService),
	                       typeid(FWindow),
	                       typeid(IRHIDevice),
	                       typeid(IRHISwapchain),
	                       typeid(FShaderCompiler),
	                       typeid(FRenderSession),
	                       typeid(FRenderFeatureRegistry),
	                       typeid(FGui),
	                       typeid(FGuiRenderer)};
	Descriptor.CreateWithContext = [Options = InOptions](FPluginContext& InContext)
	{
		return std::make_unique<FEditorPlugin>(Options, InContext);
	};
	InRegistry.Add(std::move(Descriptor));
}

FPluginSelection EditorPluginSelection(const FEditorOptions& InOptions)
{
	FPluginSelection Selection;
	Selection.Disabled = InOptions.DisabledPlugins;
	if (!InOptions.bKernelOnly)
	{
		Selection.Requested = {
		    "contact-shadows",   "editor",           "automation-scene", "storage", "automation-storage",
		    "automation-assets", "automation-local", "automation-log"};
		if (InOptions.Preferences.bRenderDocCapture)
		{
			Selection.Requested.push_back("renderdoc");
		}
	}
	return Selection;
}

void ValidateEditorStartup(const FPluginSet& InPlugins, const FEditorOptions& InOptions)
{
	if (!InPlugins.IsActive("editor"))
	{
		for (const auto& Diagnostic : InPlugins.GetDiagnostics())
		{
			if (Diagnostic.bStartupFailure)
			{
				throw std::runtime_error(Diagnostic.Id + ": " + Diagnostic.Message);
			}
		}
		if (!InOptions.Capture.empty() || !InOptions.Report.empty() || !InOptions.Benchmark.empty() ||
		    HasEditorAcceptanceRequest(InOptions))
		{
			throw std::runtime_error("Requested Editor output is unavailable: editor plugin did not start");
		}
	}
}

void ValidateEditorShutdown(FApplicationHost& InHost)
{
	InHost.GetServices().Require<FApplicationControl>().RethrowFailure();
	if (MemoryStats(EMemoryTag::Gui).LiveBytes || MemoryStats(EMemoryTag::Render).LiveBytes)
	{
		throw std::runtime_error("Editor GUI or renderer allocations survived shutdown");
	}
}
} // namespace

void RunEditorApplication(int InCount, char** InValues, FRegisterBackends InBackends, FLogHistory* InLogHistory,
                          std::shared_ptr<FStorageSettings> InStorage)
{
	const auto Options = LoadStartupOptions(InCount, InValues, InLogHistory,
	                                        InStorage ? std::move(InStorage) : CreateEditorStorage(InCount, InValues));
	FApplicationHost Host(4, 1);
	FPluginRegistry Registry;
	RegisterEditorServices(Registry, Options, std::move(InBackends));
	RegisterEditorPlugin(Registry, Options);
	Host.Start(Registry, EditorPluginSelection(Options));
	ValidateEditorStartup(Host.GetPlugins(), Options);
	Host.Run(Host.GetPlugins().IsActive("editor") ? 0 : std::max(1u, Options.Frames));
	Host.Stop();
	ValidateEditorShutdown(Host);
}
} // namespace Hyperion
