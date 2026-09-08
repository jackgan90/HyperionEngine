#include "Hyperion/DebugUI/DebugUIPlugin.h"
#include "Support/TestSupport.h"
#include <iostream>

int main()
{
	using namespace Hyperion;
	try
	{
		FGui Gui;
		Gui.FontImage();
		FProfileStatus Status;
		FDebugActions Actions;
		auto Frame = [&](std::span<const FInputEvent> InEvents)
		{
			Gui.BeginFrame({700, 600}, {1400, 1200}, 1.f / 60, InEvents);
			Gui.BeginPanel("Profiling controls", {0, 0}, {650, 550});
			Actions = DrawProfilingControls(Gui, Status);
			Gui.EndPanel();
			Gui.Render();
		};
		auto Click = [&](std::size_t InIndex)
		{
			const auto Bounds = Actions.ProfilingBounds[InIndex];
			FInputEvent Move;
			Move.Type = EEventType::MouseMove;
			Move.X = (Bounds.X + Bounds.Z) * .5f;
			Move.Y = (Bounds.Y + Bounds.W) * .5f;
			FInputEvent Button;
			Button.Type = EEventType::MouseButton;
			Button.bDown = true;
			const std::array Press{Move, Button};
			Frame(Press);
			HYP_CHECK(!Actions.ProfilingMask && !Actions.Sampling);
			Button.bDown = false;
			const std::array Release{Button};
			Frame(Release);
		};
		Frame({});
		HYP_CHECK(!Actions.ProfilingMask && Actions.ProfilingBounds[0].Z == 0);
		Status.bCompiled = true;
		Frame({});
		Frame({});
		Click(0);
		HYP_CHECK(Actions.ProfilingMask == ProfileBasicMask);
		Status.Mask = *Actions.ProfilingMask;
		Frame({});
		HYP_CHECK(!Actions.ProfilingMask);
		Click(1);
		HYP_CHECK(Actions.ProfilingMask == (ProfileBasicMask | ProfileCategoryMask(EProfileCategory::Detail)));
		Status.Mask = *Actions.ProfilingMask;
		Frame({});
		Click(2);
		HYP_CHECK(Actions.ProfilingMask == ProfileAllMask);
		Status.Mask = *Actions.ProfilingMask;
		Status.bConnected = true;
		Frame({});
		Click(3);
		HYP_CHECK(Actions.Sampling == true && !Actions.ProfilingMask);
		Status.Sampling = EProfileSampling::Requested;
		Frame({});
		Click(3);
		HYP_CHECK(Actions.Sampling == false);
		Status.Sampling = EProfileSampling::Unavailable;
		Frame({});
		Click(0);
		HYP_CHECK(Actions.ProfilingMask == (ProfileAllMask & ~ProfileBasicMask));
		std::cout << "Profiling controls: unavailable build, independent toggles, state feedback and high DPI passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
