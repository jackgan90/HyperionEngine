#include "ViewerApplication.h"
#if HYP_ENABLE_RENDERDOC
#include "Hyperion/RenderDoc/RenderDocPlugin.h"
#endif
#include <algorithm>

namespace Hyperion
{
void FViewerApplication::InitializeCapture()
{
	const bool Enabled =
	    std::find(Settings.Plugins.begin(), Settings.Plugins.end(), "renderdoc") != Settings.Plugins.end();
#if HYP_ENABLE_RENDERDOC
	Metrics.FrameCapture.Compiled = true;
	Metrics.FrameCapture.Status = "RenderDoc disabled (enable plugin, save and restart)";
	if (Enabled)
	{
		FPluginRegistry Registry;
		RegisterRenderDocPlugin(
		    Registry,
		    {std::filesystem::path(std::u8string(Settings.RenderDocLibrary.begin(), Settings.RenderDocLibrary.end())),
		     std::filesystem::path(std::u8string(Settings.RenderDocOutput.begin(), Settings.RenderDocOutput.end())),
		     Settings.ModelSource.empty() ? "Triangle" : "Model"});
		const std::array<std::string, 1> Ids{"renderdoc"};
		// Capture hooks must be installed before creating a window or any graphics objects.
		StartupPlugins = std::make_unique<FPluginSet>(Registry.Activate(Ids));
		for (const auto& Plugin : StartupPlugins->GetInstances())
		{
			FrameCapture = &static_cast<FRenderDocPlugin&>(*Plugin).Capture();
		}
	}
#else
	if (Enabled)
	{
		throw std::runtime_error("RenderDoc plugin is not compiled; configure HYP_ENABLE_RENDERDOC=ON");
	}
#endif
}

void FViewerApplication::UpdateCaptureStatus()
{
#if HYP_ENABLE_RENDERDOC
	if (FrameCapture)
	{
		const auto Status = FrameCapture->Status();
		Metrics.FrameCapture.Available = Status.Available;
		Metrics.FrameCapture.Busy =
		    Status.State == EFrameCaptureState::Pending || Status.State == EFrameCaptureState::Capturing;
		Metrics.FrameCapture.Status = Status.Message;
		const auto Path = Status.LastCapture.u8string();
		Metrics.FrameCapture.LastCapture.assign(reinterpret_cast<const char*>(Path.data()), Path.size());
		Metrics.FrameCapture.OpenStatus = Status.ReplayMessage;
	}
#endif
}

void FViewerApplication::HandleCaptureActions(const FDebugActions& InActions, bool InScheduled)
{
#if HYP_ENABLE_RENDERDOC
	if (FrameCapture && (InActions.CaptureRdc || (InScheduled && !Options.ExerciseRdcUi)))
	{
		FrameCapture->RequestCapture();
	}
	if (FrameCapture && InActions.OpenRdc)
	{
		FrameCapture->OpenLastCapture();
	}
#else
	(void)InActions;
	(void)InScheduled;
#endif
}

void FViewerApplication::ExerciseCaptureInput(bool InScheduled, std::vector<FInputEvent>& InEvents)
{
	if (Options.ExerciseRdcUi && (InScheduled || RdcMouseDown))
	{
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (RdcButtonBounds.X + RdcButtonBounds.Z) * .5f;
		Move.Y = (RdcButtonBounds.Y + RdcButtonBounds.W) * .5f;
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.Down = InScheduled;
		InEvents.push_back(Move);
		InEvents.push_back(Button);
		RdcMouseDown = InScheduled;
	}
}
} // namespace Hyperion
