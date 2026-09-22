#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"
#include <array>

namespace
{
using namespace Hyperion;

struct FContentFixture
{
	FGui Gui;
	std::array<FVec4, 2> Bounds;
	int Selected = -1;
	unsigned Opens{};

	FContentFixture()
	{
		Gui.UseEditorStyle();
		Frame();
		Frame();
	}

	void Frame(std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, InEvents);
		Gui.DockSpace({"Scene", {}, {}, {}, "Content"});
		bool bOpen = true;
		Gui.BeginWindow("Scene", bOpen);
		Gui.Image(42);
		Gui.EndWindow();
		Gui.BeginWindow("Content", bOpen);
		Gui.BeginScrollRegion("Contents", 0);
		if (Gui.BeginTileGrid("Tiles"))
		{
			for (int Index = 0; Index < 2; ++Index)
			{
				Gui.NextColumn();
				const auto Name = std::to_string(Index) + ".hasset";
				bool bDoubleClicked{};
				if (Gui.FileTile(Name.c_str(), Name.c_str(), false, Selected == Index, bDoubleClicked))
				{
					Selected = Index;
					Opens += bDoubleClicked ? 1 : 0;
				}
				Bounds[Index] = Gui.LastItemBounds();
			}
			Gui.EndTable();
		}
		Gui.EndScrollRegion();
		Gui.EndWindow();
		Gui.Render();
	}

	void Move(int InIndex)
	{
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (Bounds[InIndex].X + Bounds[InIndex].Z) / 2;
		Move.Y = (Bounds[InIndex].Y + Bounds[InIndex].W) / 2;
		Frame({&Move, 1});
	}

	void Click()
	{
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = true;
		Frame({&Button, 1});
		Button.bDown = false;
		Frame({&Button, 1});
	}
};
} // namespace

void CheckContentTiles()
{
	FContentFixture Test;
	Test.Move(0);
	Test.Click();
	HYP_CHECK(Test.Selected == 0 && Test.Opens == 0);
	Test.Click();
	HYP_CHECK(Test.Opens == 1);
	Test.Move(1);
	Test.Click();
	HYP_CHECK(Test.Selected == 1 && Test.Opens == 1);
	Test.Click();
	HYP_CHECK(Test.Opens == 2);

	FContentFixture Batched;
	Batched.Move(0);
	FInputEvent Down;
	Down.Type = EEventType::MouseButton;
	Down.bDown = true;
	FInputEvent Up = Down;
	Up.bDown = false;
	Batched.Frame(std::array{Down, Up, Down, Up});
	for (unsigned Index = 0; Index < 5; ++Index)
	{
		Batched.Frame();
	}
	HYP_CHECK(Batched.Selected == 0 && Batched.Opens == 1);
}
