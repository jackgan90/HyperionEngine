#include "EditorApplication.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Editor/EditorPlugin.h"

namespace Hyperion
{
void RunEditorApplication(int InCount, char** InValues, FRegisterBackends InBackends)
{
	const auto Options = ParseEditorOptions(InCount, InValues);
	FApplicationHost Host(4, 1);
	FPluginRegistry Registry;
	RegisterAssetServices(Registry, {Options.Mounts, false});
	RegisterWindowServices(Registry, {"Hyperion Editor", {1600, 960}, Options.bHidden, true});
	RegisterGraphicsServices(
	    Registry, {std::move(InBackends), "d3d12", std::filesystem::path(HYP_SOURCE_DIR) / "out/shader-cache", true});
	const bool bPersistGui = !Options.bExercise && Options.Benchmark.empty() && Options.ExerciseDocument.empty() &&
	                         Options.ExerciseViews.empty();
	RegisterGuiServices(
	    Registry, {true, "/Engine/Fonts/RobotoMedium.ttf", 15, bPersistGui ? Options.Layout : std::filesystem::path{},
	               bPersistGui ? Options.UiPreferences : std::filesystem::path{}, Options.ApplicationScale});
	RegisterContactShadowServices(Registry);
	FPluginDescriptor Descriptor;
	Descriptor.Id = "editor";
	Descriptor.Dependencies = {"gui"};
	Descriptor.After = {"contact-shadows"};
	Descriptor.Requires = {typeid(FApplicationControl),
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
		Selection.Requested = {"contact-shadows", "editor"};
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
		    !Options.ExerciseDocument.empty() || !Options.ExerciseViews.empty() || Options.bExercise)
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
