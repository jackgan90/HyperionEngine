#include "EditorApplication.h"
#include "Hyperion/AutomationHost/AutomationPlugin.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Editor/EditorPlugin.h"
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/RenderDoc/RenderDocPlugin.h"
#endif

namespace Hyperion
{
void RunEditorApplication(int InCount, char** InValues, FRegisterBackends InBackends)
{
	auto Options = ParseEditorOptions(InCount, InValues);
	try
	{
		Options.Preferences = LoadEditorPreferences(Options.PreferencesPath);
	}
	catch (const std::exception& Failure)
	{
		Options.PreferenceError = "Could not load editor preferences: " + std::string(Failure.what());
		Log(ELogLevel::Warning, Options.PreferenceError);
	}
	FApplicationHost Host(4, 1);
	FPluginRegistry Registry;
	RegisterAutomationServices(Registry);
	RegisterSceneAutomation(Registry);
	RegisterAssetAutomation(Registry);
	RegisterAutomationLocal(Registry, "Editor");
#if HYP_ENABLE_RENDERDOC
	RegisterRenderDocPlugin(Registry, {{}, std::filesystem::path(HYP_SOURCE_DIR) / "out/captures", "Editor"});
#endif
	const bool bInteractive =
	    Options.ExerciseAssets.empty() && Options.ExerciseContent.empty() && !Options.bExercise &&
	    !Options.bExerciseGizmo && !Options.bExercisePicking && !Options.bExerciseMultiSelection &&
	    Options.Benchmark.empty() && Options.ExerciseDocument.empty() && Options.ExerciseViews.empty() &&
	    Options.ExercisePlacement.empty() && Options.ExerciseOutlines.empty() && Options.ExerciseCapture.empty();
	const auto RestoredRoot = !Options.AssetRoot && bInteractive && !Options.Preferences.AssetRoot.empty()
	                              ? std::optional(Options.Preferences.AssetRoot)
	                              : std::nullopt;
	RegisterAssetServices(Registry, {Options.EngineContent, Options.AssetRoot ? Options.AssetRoot : RestoredRoot,
	                                 Options.bReadOnly, RestoredRoot.has_value()});
	RegisterWindowServices(Registry, {"Hyperion Editor", {1600, 960}, Options.bHidden, true});
	RegisterGraphicsServices(
	    Registry, {std::move(InBackends), "d3d12", std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache", true});
	const bool bPersistGui = Options.ExerciseAssets.empty() && !Options.bExercise && !Options.bExerciseGizmo &&
	                         !Options.bExercisePicking && !Options.bExerciseMultiSelection &&
	                         Options.Benchmark.empty() && Options.ExerciseDocument.empty() &&
	                         Options.ExerciseViews.empty() && Options.ExercisePlacement.empty() &&
	                         Options.ExerciseOutlines.empty() && Options.ExerciseCapture.empty();
	const bool bPersistContentLayout = bPersistGui && Options.ExerciseContent.empty();
	RegisterGuiServices(Registry, {true, "/Engine/Fonts/RobotoMedium.ttf", 15,
	                               bPersistContentLayout ? Options.Layout : std::filesystem::path{},
	                               bPersistContentLayout ? Options.UiPreferences : std::filesystem::path{},
	                               Options.ApplicationScale});
	RegisterContactShadowServices(Registry);
	FPluginDescriptor Descriptor;
	Descriptor.Id = "editor";
	Descriptor.Provides = {typeid(FSceneEditDocument),     typeid(ISceneDocumentHost),    typeid(IAssetWorkspace),
	                       typeid(IAssetPreviewWorkspace), typeid(ISceneViewport),        typeid(IScenePlacement),
	                       typeid(IRenderOutput),          typeid(IRenderCaptureControl), typeid(IRenderDiagnostics),
	                       typeid(IApplicationClose)};
	Descriptor.Dependencies = {"gui"};
	Descriptor.After = {"contact-shadows"};
#if HYP_ENABLE_RENDERDOC
	Descriptor.Optional = {typeid(FFrameCapture)};
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
	Descriptor.CreateWithContext = [Options](FPluginContext& InContext)
	{
		return std::make_unique<FEditorPlugin>(Options, InContext);
	};
	Registry.Add(std::move(Descriptor));
	FPluginSelection Selection;
	Selection.Disabled = Options.DisabledPlugins;
	if (!Options.bKernelOnly)
	{
		Selection.Requested = {"contact-shadows", "editor", "automation-scene", "automation-assets",
		                       "automation-local"};
		if (Options.Preferences.bRenderDocCapture)
		{
			Selection.Requested.push_back("renderdoc");
		}
	}
	Host.Start(Registry, Selection);
	if (!Host.GetPlugins().IsActive("editor"))
	{
		for (const auto& Diagnostic : Host.GetPlugins().GetDiagnostics())
		{
			if (Diagnostic.bStartupFailure)
			{
				throw std::runtime_error(Diagnostic.Id + ": " + Diagnostic.Message);
			}
		}
		if (!Options.Capture.empty() || !Options.Report.empty() || !Options.Benchmark.empty() ||
		    !Options.ExerciseDocument.empty() || !Options.ExerciseViews.empty() || Options.bExercise ||
		    Options.bExerciseGizmo || Options.bExercisePicking || Options.bExerciseMultiSelection ||
		    !Options.ExercisePlacement.empty() || !Options.ExerciseOutlines.empty() ||
		    !Options.ExerciseCapture.empty() || !Options.ExerciseContent.empty() || !Options.ExerciseAssets.empty())
		{
			throw std::runtime_error("Requested Editor output is unavailable: editor plugin did not start");
		}
	}
	Host.Run(Host.GetPlugins().IsActive("editor") ? 0 : std::max(1u, Options.Frames));
	Host.Stop();
	Host.GetServices().Require<FApplicationControl>().RethrowFailure();
	if (MemoryStats(EMemoryTag::Gui).LiveBytes || MemoryStats(EMemoryTag::Render).LiveBytes)
	{
		throw std::runtime_error("Editor GUI or renderer allocations survived shutdown");
	}
}
} // namespace Hyperion
