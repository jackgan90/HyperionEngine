#include "Hyperion/DebugUI/DebugUIPlugin.h"
#include "Support/TestSupport.h"
#include <array>
#include <iostream>

int main()
{
	using namespace Hyperion;
	try
	{
		FGui Gui;
		Gui.FontImage();
		FAppSettings Settings;
		FFrameCaptureMetrics Metrics;
		Metrics.Compiled = true;
		FDebugActions Actions;
		auto Frame = [&](std::span<const FInputEvent> InEvents)
		{
			Gui.BeginFrame({700, 600}, {1400, 1200}, 1.f / 60, InEvents);
			Gui.BeginPanel("Capture controls", {0, 0}, {650, 550});
			Actions = DrawFrameCaptureControls(Gui, Settings, Metrics);
			Gui.EndPanel();
			return Gui.Render();
		};
		auto Click = [&](FVec4 InBounds)
		{
			FInputEvent Move;
			Move.Type = EEventType::MouseMove;
			Move.X = (InBounds.X + InBounds.Z) * .5f;
			Move.Y = (InBounds.Y + InBounds.W) * .5f;
			FInputEvent Button;
			Button.Type = EEventType::MouseButton;
			Button.Down = true;
			const std::array Press{Move, Button};
			Frame(Press);
			HYP_CHECK(!Actions.CaptureRdc && !Actions.OpenRdc);
			Button.Down = false;
			const std::array Release{Button};
			Frame(Release);
		};
		Frame({});
		Frame({});
		Click(Actions.CaptureRdcBounds);
		HYP_CHECK(!Actions.CaptureRdc);
		Metrics.Available = true;
		Frame({});
		Click(Actions.CaptureRdcBounds);
		HYP_CHECK(Actions.CaptureRdc && !Actions.OpenRdc && !Actions.Capture);
		Frame({});
		HYP_CHECK(!Actions.CaptureRdc);
		Metrics.Busy = true;
		Frame({});
		Click(Actions.CaptureRdcBounds);
		HYP_CHECK(!Actions.CaptureRdc);
		Metrics.Busy = false;
		Frame({});
		Click(Actions.OpenRdcBounds);
		HYP_CHECK(!Actions.OpenRdc);
		Metrics.LastCapture = "example.rdc";
		Frame({});
		Click(Actions.OpenRdcBounds);
		HYP_CHECK(Actions.OpenRdc && !Actions.CaptureRdc);
		HYP_CHECK(!Settings.RenderDocAutoOpen);
		Frame({});
		Click(Actions.AutoOpenRdcBounds);
		HYP_CHECK(Settings.RenderDocAutoOpen);
		Settings.RenderDocLibrary = "custom/renderdoc.dll";
		Settings.RenderDocOutput = "capture folder";
		SaveSettings("capture-control-settings.json", Settings);
		const auto Restored = LoadSettings("capture-control-settings.json");
		HYP_CHECK(Restored.RenderDocAutoOpen);
		HYP_CHECK(Restored.RenderDocLibrary == Settings.RenderDocLibrary);
		HYP_CHECK(Restored.RenderDocOutput == Settings.RenderDocOutput);
		std::cout << "Capture controls: disabled, busy, press/release, independent actions, persistence and high DPI "
		             "passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
