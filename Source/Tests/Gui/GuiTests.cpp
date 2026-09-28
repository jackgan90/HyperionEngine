#include "Hyperion/Config/AppSettings.h"
#include "Hyperion/Core/Core.h"
#include "Hyperion/Gui/Gui.h"
#include <array>
#include <iostream>
#include <stdexcept>

void CheckColorControls();
void CheckDragControls();
void CheckApplicationScale();
void CheckObjectDrag();
void CheckMixedControls();
void CheckPathDisplay();
void CheckContentTiles();
void CheckSectionControls();
void CheckAssetReferenceControls();
void CheckVisibilityControls();

namespace
{
using namespace Hyperion;

void Check(bool bInB, const char* InM)
{
	if (!bInB)
	{
		throw std::runtime_error(InM);
	}
}

void CheckAnchoredPopup()
{
	for (const float Scale : {1.f, 2.f})
	{
		for (const float Left : {20.f, 650.f})
		{
			FGui Gui;
			Gui.UseEditorStyle();
			Gui.SetApplicationScale(Scale);
			bool bSelected = false;
			FVec4 OptionBounds;
			for (int Frame = 0; Frame < 6; ++Frame)
			{
				std::vector<FInputEvent> Events;
				if (Frame == 3 || Frame == 4)
				{
					FInputEvent Move;
					Move.Type = EEventType::MouseMove;
					Move.X = OptionBounds.X + Gui.Scale(8);
					Move.Y = (OptionBounds.Y + OptionBounds.W) * .5f;
					Events.push_back(Move);
					FInputEvent Click;
					Click.Type = EEventType::MouseButton;
					Click.bDown = Frame == 3;
					Events.push_back(Click);
				}
				Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, Events);
				Gui.BeginPanel("Popup anchor", {Left, 20}, {150, 500});
				Gui.Button("Stats...");
				const auto Anchor = Gui.LastItemBounds();
				if (Frame == 0)
				{
					Gui.OpenPopup("Stats");
				}
				Check(Gui.BeginPopup("Stats", Anchor), "Anchored popup opens");
				Gui.Text("Profiling HUD categories");
				const auto Bounds = Gui.LastItemBounds();
				Gui.Checkbox("Show profiling HUD", bSelected);
				OptionBounds = Gui.LastItemBounds();
				if (Frame == 5)
				{
					Check(bSelected, "Anchored popup option responds to clicks after edge placement");
				}
				if (Frame >= 2)
				{
					Check(Bounds.X >= 0 && Bounds.Z <= 800, "Anchored popup content stays inside the viewport");
					Check(Bounds.Y >= Anchor.W && Bounds.Y <= Anchor.W + Gui.Scale(16),
					      "Anchored popup unfolds below its button");
					if (Left == 20.f)
					{
						Check(Bounds.X >= Anchor.X && Bounds.X <= Anchor.X + Gui.Scale(16),
						      "Anchored popup aligns left when space permits");
					}
				}
				Gui.EndPopup();
				Gui.EndPanel();
				const auto Draw = Gui.Render();
				if (Frame >= 2)
				{
					bool bTitleVisible = false;
					for (const auto& Command : Draw.Commands)
					{
						for (std::uint32_t Index = 0; Index < Command.IndexCount; ++Index)
						{
							const auto& Position =
							    Draw.Vertices[Command.VertexOffset + Draw.Indices[Command.FirstIndex + Index]].Position;
							bTitleVisible |= Position.X >= Bounds.X && Position.X < Bounds.X + Gui.Scale(20) &&
							                 Position.Y > Bounds.Y && Position.Y < Bounds.W &&
							                 Position.X >= Command.Clip.X && Position.X < Command.Clip.Z &&
							                 Position.Y >= Command.Clip.Y && Position.Y < Command.Clip.W;
						}
					}
					Check(bTitleVisible, "Popup title is rendered inside its clip rectangle");
				}
			}
		}
	}
}

void CheckActionLayout()
{
	FGui Gui;
	Gui.UseEditorStyle();
	for (const float Scale : {1.f, 2.f})
	{
		Gui.SetApplicationScale(Scale);
		for (const FVec2 Size : {FVec2{400, 300}, FVec2{560, 420}})
		{
			for (const bool bError : {false, true})
			{
				std::string Message;
				if (bError)
				{
					for (int Index = 0; Index < 30; ++Index)
					{
						Message += "An import error with details that can wrap.\n";
					}
				}
				for (int Frame = 0; Frame < 3; ++Frame)
				{
					Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, {});
					Gui.BeginPanel("Action layout", {0, 0}, Size);
					Gui.BeginActionLayout("Form", Message);
					for (int Index = 0; Index < 80; ++Index)
					{
						Gui.Text("Scrollable property");
					}
					Gui.EndActionLayout("Import", Message);
					const auto Bounds = Gui.LastItemBounds();
					Check(Bounds.Z <= Size.X && Bounds.W <= Size.Y, "Action stays visible during resize and scaling");
					// Window scrollbars use the previous frame's content size while resize/scale settles.
					if (Frame == 2)
					{
						Check(Bounds.X > Size.X * .5f && Bounds.Z >= Size.X - Gui.Scale(16),
						      "Action button stays at the right edge");
						Check(Bounds.Y > Size.Y * .5f && Bounds.W >= Size.Y - Gui.Scale(16),
						      "Action button stays at the bottom with long content and errors");
					}
					Gui.EndPanel();
					Gui.Render();
				}
			}
		}
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
			bool bEnabled = false;
			FAppSettings Settings;
			FVec4 Box;
			FVec4 Property;
			auto Frame = [&](std::span<const FInputEvent> InEvents)
			{
				Gui.BeginFrame({640, 480}, {1280, 960}, 1.f / 60, InEvents);
				Gui.BeginPanel("Test", {0, 0}, {400, 300});
				Gui.Checkbox("Enabled", bEnabled);
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
				Down.bDown = true;
				Down.Button = 0;
				std::array Events{Move, Down};
				Frame(Events);
				Down.bDown = false;
				std::array Release{Down};
				Frame(Release);
			};
			Frame({});
			Click(Box, .05f);
			Frame({});
			Check(bEnabled, "Normalized mouse toggles checkbox");
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
		CheckColorControls();
		CheckDragControls();
		CheckObjectDrag();
		CheckMixedControls();
		CheckPathDisplay();
		CheckContentTiles();
		CheckSectionControls();
		CheckAssetReferenceControls();
		CheckVisibilityControls();
		CheckApplicationScale();
		CheckActionLayout();
		CheckAnchoredPopup();
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
