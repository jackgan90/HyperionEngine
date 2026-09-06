#include "hyperion/Core.h"
#include "hyperion/Gui.h"
#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace Hyperion;

void Check(bool InB, const char* InM)
{
	if (!InB)
	{
		throw std::runtime_error(InM);
	}
}
} // namespace

int main()
{
	using namespace Hyperion;
	try
	{
		{
			FGui Gui;
			auto Font = Gui.FontImage();
			Check(Font.Width && Font.Height, "Font atlas");
			bool Enabled = false;
			FAppSettings Settings;
			FVec4 Box;
			FVec4 Property;
			auto Frame = [&](std::span<const FInputEvent> InEvents)
			{
				Gui.BeginFrame({640, 480}, {1280, 960}, 1.f / 60, InEvents);
				Gui.BeginPanel("Test", {0, 0}, {400, 300});
				Gui.Checkbox("Enabled", Enabled);
				Box = Gui.LastItemBounds();
				constexpr std::array<std::string_view, 1> Ids{"triangle_scale"};
				Gui.EditProperties(SettingsType(), &Settings, Ids);
				Property = Gui.LastItemBounds();
				Gui.EndPanel();
				return Gui.Render();
			};
			auto First = Frame({});
			Check(!First.Vertices.empty() && !First.Indices.empty() && First.FramebufferScale.X == 2,
			      "Owned GUI draw data");
			auto Saved = First.Vertices;
			auto Click = [&](FVec4 InBounds, float InXFraction)
			{
				FInputEvent Move;
				Move.Type = EEventType::MouseMove;
				Move.X = InBounds.X + (InBounds.Z - InBounds.X) * InXFraction;
				Move.Y = (InBounds.Y + InBounds.W) * .5f;
				FInputEvent Down;
				Down.Type = EEventType::MouseButton;
				Down.Down = true;
				Down.Button = 0;
				std::array Events{Move, Down};
				Frame(Events);
				Down.Down = false;
				std::array Release{Down};
				Frame(Release);
			};
			Frame({});
			Click(Box, .05f);
			Frame({});
			Check(Enabled, "Normalized mouse toggles checkbox");
			double Original = Settings.TriangleScale;
			Click(Property, .1f);
			Check(Settings.TriangleScale != Original, "Reflection control updates settings");
			SaveSettings("gui-settings.json", Settings);
			auto Restored = LoadSettings("gui-settings.json");
			Check(Restored.TriangleScale == Settings.TriangleScale, "Edited settings persist");
			Check(First.Vertices.size() == Saved.size() && First.Vertices[0].Position.X == Saved[0].Position.X,
			      "Draw data survives later GUI frames");
			for (const auto& Cmd : First.Commands)
			{
				Check(std::size_t(Cmd.FirstIndex) + Cmd.IndexCount <= First.Indices.size(), "Copied index range");
			}
		}
		Check(MemoryStats(EMemoryTag::Gui).LiveBytes == 0, "GUI allocations released");
		std::cout << "GUI input, reflection, owned draw data and cleanup passed\n";
		return 0;
	}
	catch (const std::exception& E)
	{
		std::cerr << E.what() << '\n';
		return 1;
	}
}
