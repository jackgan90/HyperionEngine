#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"
#include <algorithm>
#include <iostream>

namespace
{
void CheckWorkspaceScaleTransition()
{
	using namespace Hyperion;
	FGui Gui;
	Gui.UseEditorStyle();
	bool bOpen = true;
	const auto Frame = [&]
	{
		Gui.BeginFrame({1600, 960}, {1600, 960}, 1.f / 60, {});
		if (Gui.BeginToolbar())
		{
			Gui.Button("Open Scene");
		}
		Gui.EndToolbar();
		Gui.StatusBar("Ready");
		Gui.DockSpace({"View", "Objects", "Details", "Content"});
		Gui.BeginWindow("View", bOpen);
		const auto Region = Gui.Image(42);
		Gui.EndWindow();
		Gui.Render();
		return Region.Bounds;
	};
	Frame();
	Frame();
	for (const float Scale : {2.f, 1.f, 1.5f, 1.25f})
	{
		Gui.SetApplicationScale(Scale);
		const auto First = Frame();
		const auto Next = Frame();
		// Bar reservations must be current immediately. Automatic scrollbar
		// visibility may settle after consuming the preceding frame's content size.
		HYP_CHECK(First.Y == Next.Y && First.W == Next.W);
		for (unsigned Index = 0; Index < 20; ++Index)
		{
			const auto Stable = Frame();
			HYP_CHECK(Stable.X == Next.X && Stable.Y == Next.Y && Stable.Z == Next.Z && Stable.W == Next.W);
		}
	}
}
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		CheckWorkspaceScaleTransition();
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
		const auto OriginalBounds = Region.Bounds;
		Gui.SetApplicationScale(2);
		Frame(false);
		const auto ScaledLayout = Gui.SaveLayout();
		HYP_CHECK(ScaledLayout.substr(ScaledLayout.find("[Docking][Data]")) ==
		          Layout.substr(Layout.find("[Docking][Data]")));
		HYP_CHECK(Region.Bounds.Z - Region.Bounds.X < OriginalBounds.Z - OriginalBounds.X);
		Gui.SetApplicationScale(1);
		Frame(false);
		HYP_CHECK(Region.Bounds.X == OriginalBounds.X && Region.Bounds.Z == OriginalBounds.Z);
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
