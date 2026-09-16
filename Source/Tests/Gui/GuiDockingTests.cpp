#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <iostream>

int main()
{
	using namespace Hyperion;
	try
	{
		FGui Gui;
		Gui.UseEditorStyle();
		Gui.FontImage();
		bool bOpen = true;
		FGuiImageRegion Region;
		const auto Frame = [&](bool bInReset)
		{
			Gui.BeginFrame({800, 600}, {1600, 1200}, 1.f / 60, {});
			Gui.DockSpace({"View", "Objects", "Details", "Content"}, bInReset);
			if (Gui.BeginWindow("View", bOpen))
			{
				Region = Gui.Image(42);
			}
			Gui.EndWindow();
			return Gui.Render();
		};
		Frame(true);
		const auto First = Frame(false);
		HYP_CHECK(First.FramebufferScale.X == 2 && Region.Bounds.Z > Region.Bounds.X);
		HYP_CHECK(std::any_of(First.Commands.begin(), First.Commands.end(),
		                      [](const auto& InCommand)
		                      {
			                      return InCommand.TextureId == 42;
		                      }));
		const auto Layout = Gui.SaveLayout();
		HYP_CHECK(Layout.find("[Docking][Data]") != std::string::npos);
		Gui.LoadLayout(Layout);
		Frame(false);
		HYP_CHECK(!First.Vertices.empty() && First.Commands.back().TextureId != 0);
		std::cout << "Docked layout, restored workspace, DPI and owned image commands passed\n";
		return 0;
	}
	catch (const std::exception& Error)
	{
		std::cerr << Error.what() << '\n';
		return 1;
	}
}
