#include "Hyperion/SceneViewer/SkyControls.h"
#include "Support/TestSupport.h"
#include <iostream>

using namespace Hyperion;

int main()
{
	try
	{
		FGui Gui;
		Gui.FontImage();
		FSceneEnvironmentLight Light;
		std::string Path;
		const std::array<std::string, 3> Choices{"F:/Cloudy.hasset", "F:/Dusk.hasset", "F:/Clear.hasset"};
		FSkyControlResult Controls;
		const auto Frame = [&](std::span<const FInputEvent> InEvents)
		{
			Gui.BeginFrame({700, 600}, {1400, 1200}, 1.f / 60, InEvents);
			Gui.BeginPanel("Sky controls", {0, 0}, {650, 550});
			Controls = DrawSkyAssetControls(Gui, Light, Path, Choices);
			Gui.EndPanel();
			return Gui.Render();
		};
		const auto Click = [&](FVec4 InBounds)
		{
			FInputEvent Move;
			Move.Type = EEventType::MouseMove;
			Move.X = InBounds.X + 12;
			Move.Y = (InBounds.Y + InBounds.W) * .5f;
			FInputEvent Button;
			Button.Type = EEventType::MouseButton;
			Button.bDown = true;
			Frame(std::array{Move, Button});
			Button.bDown = false;
			Frame(std::array{Button});
		};
		const auto Key = [&](EKey InKey)
		{
			FInputEvent Event;
			Event.Type = EEventType::Key;
			Event.Key = InKey;
			Event.bDown = true;
			Frame(std::array{Event});
			Event.bDown = false;
			Frame(std::array{Event});
		};
		Frame({});
		const auto Image = Frame({});
		HYP_CHECK(Image.FramebufferScale.X == 2 && !Image.Vertices.empty());
		Click(Controls.ApplyBounds);
		HYP_CHECK(!Light.Sky && !Controls.bChanged);
		Click(Controls.PathBounds);
		FInputEvent Text;
		Text.Type = EEventType::Text;
		Text.Text = "F:/Custom.hasset";
		Frame(std::array{Text});
		Click(Controls.ApplyBounds);
		std::cout << "sky path=" << Path << " selected=" << (Light.Sky ? Light.Sky->Path : "none")
		          << " changed=" << Controls.bChanged << '\n';
		HYP_CHECK(Controls.bChanged && Light.Sky->Path == "F:/Custom.hasset");
		HYP_CHECK(Light.Source == ESceneEnvironmentSource::SkyAsset);
		Click(Controls.VisibilityBounds);
		HYP_CHECK(Controls.bChanged && !Light.bVisible);
		Click(Controls.AssetBounds);
		Key(EKey::Down);
		Key(EKey::Enter);
		HYP_CHECK(Light.Sky->Path == Choices[0]);
		for (const auto& Choice : Choices)
		{
			Path = Choice;
			Frame({});
			Click(Controls.ApplyBounds);
			HYP_CHECK(Controls.bChanged && Light.Sky->Path == Choice);
			const auto Restored = ReadValue<FSceneEnvironmentLight>(WriteValue(Light));
			HYP_CHECK(Restored == Light);
		}
		std::cout << "Sky GUI custom text, preset selection, apply, visibility and high-DPI input passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
