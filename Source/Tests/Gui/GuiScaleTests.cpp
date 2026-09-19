#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"
#include <array>
#include <cmath>
#include <limits>

namespace
{
void CheckScaleTextInput()
{
	using namespace Hyperion;
	FGui Gui;
	FVec4 Bounds;
	const auto Frame = [&](std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, InEvents);
		Gui.BeginPanel("Scale input", {0, 0}, {700, 500});
		Gui.SetNextItemWidth(160);
		Gui.ApplicationScaleControl("Custom");
		Bounds = Gui.LastItemBounds();
		Gui.EndPanel();
		Gui.Render();
	};
	Frame();
	Frame();
	const std::array Texts{"1.75", "9", "-2", "nan"};
	const std::array Expected{1.75f, 2.f, 1.f, 1.f};
	for (std::size_t Index = 0; Index < Texts.size(); ++Index)
	{
		// Let the prior text interaction deactivate before starting another one.
		Frame();
		Frame();
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = Bounds.X + Gui.Scale(60);
		Move.Y = (Bounds.Y + Bounds.W) / 2;
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = EKey::A;
		Key.Modifiers = 1;
		Frame(std::array{Move, Key});
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = true;
		Frame(std::array{Button});
		Button.bDown = false;
		Frame(std::array{Button});
		Key.bDown = true;
		Frame(std::array{Key});
		Key.bDown = false;
		Key.Modifiers = 0;
		FInputEvent Text;
		Text.Type = EEventType::Text;
		Text.Text = Texts[Index];
		Frame(std::array{Key, Text});
		Key.Key = EKey::Enter;
		Key.bDown = true;
		Frame(std::array{Key});
		Key.bDown = false;
		Frame(std::array{Key});
		if (Gui.ApplicationScale() != Expected[Index])
		{
			throw std::runtime_error("Scale input " + std::string(Texts[Index]) + " produced " +
			                         std::to_string(Gui.ApplicationScale()));
		}
	}
}

void CheckScaleStability()
{
	using namespace Hyperion;
	FGui Gui;
	Gui.UseEditorStyle();
	Gui.SetApplicationScale(1.25f);
	FVec4 Bounds;
	const auto Frame = [&](std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({1600, 960}, {1600, 960}, 1.f / 60, InEvents);
		Gui.BeginPanel("Scale interaction", {200, 100}, {700, 600});
		Gui.SetNextItemWidth(160);
		Gui.ApplicationScaleControl("Custom");
		Bounds = Gui.LastItemBounds();
		Gui.EndPanel();
		return Gui.Render();
	};
	Frame();
	Frame();
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = Bounds.X + Gui.Scale(150);
	Move.Y = (Bounds.Y + Bounds.W) / 2;
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.Button = 0;
	Button.bDown = true;
	Frame(std::array{Move, Button});
	Move.X += 40;
	Frame(std::array{Move});
	HYP_CHECK(Gui.ApplicationScale() > 1.25f);
	const float Settled = Gui.ApplicationScale();
	const auto Stable = Frame();
	const auto StableBounds = Bounds;
	for (unsigned Index = 0; Index < 20; ++Index)
	{
		const auto Data = Frame();
		HYP_CHECK(Gui.ApplicationScale() == Settled && Data.FontAtlas == Stable.FontAtlas);
		HYP_CHECK(Bounds.X == StableBounds.X && Bounds.Y == StableBounds.Y && Bounds.Z == StableBounds.Z &&
		          Bounds.W == StableBounds.W);
	}
	Move.X -= 20;
	Frame(std::array{Move});
	HYP_CHECK(Gui.ApplicationScale() < Settled);
	Button.bDown = false;
	Frame(std::array{Button});
	const float Released = Gui.ApplicationScale();
	Frame();
	HYP_CHECK(Gui.ApplicationScale() == Released);
}

void CheckButtonWrapping()
{
	using namespace Hyperion;
	FGui Gui;
	FVec4 First;
	FVec4 Second;
	const auto Frame = [&](float InScale)
	{
		Gui.SetApplicationScale(InScale);
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, {});
		Gui.BeginPanel("Wrap", {0, 0}, {350, 500});
		Gui.Button("First action");
		First = Gui.LastItemBounds();
		Gui.SameLineIfFits("Second action");
		Gui.Button("Second action");
		Second = Gui.LastItemBounds();
		Gui.EndPanel();
		Gui.Render();
	};
	Frame(1);
	Frame(1);
	HYP_CHECK(First.Y == Second.Y && Second.X > First.Z);
	Frame(2);
	HYP_CHECK(Second.Y >= First.W && Second.Z < 350);
}
} // namespace

void CheckApplicationScale()
{
	using namespace Hyperion;
	CheckButtonWrapping();
	CheckScaleStability();
	CheckScaleTextInput();
	FGui Gui;
	Gui.UseEditorStyle();
	bool bChecked{};
	FVec4 Bounds;
	const auto Frame = [&](float InDensity, std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 600}, {unsigned(800 * InDensity), unsigned(600 * InDensity)}, 1.f / 60, InEvents);
		Gui.BeginPanel("Scale", {0, 0}, {800, 600});
		Gui.Checkbox("Scale target", bChecked);
		Bounds = Gui.LastItemBounds();
		Gui.EndPanel();
		return Gui.Render();
	};
	Frame(1);
	const auto Original = Frame(1);
	const auto OriginalBounds = Bounds;
	HYP_CHECK(Original.FontAtlas && Frame(1).FontAtlas == Original.FontAtlas);
	for (const float Invalid :
	     {0.f, .5f, 2.1f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
	{
		bool bRejected{};
		try
		{
			Gui.SetApplicationScale(Invalid);
		}
		catch (const std::invalid_argument&)
		{
			bRejected = true;
		}
		HYP_CHECK(bRejected && Gui.ApplicationScale() == 1);
	}
	for (unsigned Iteration = 0; Iteration < 3; ++Iteration)
	{
		Gui.SetApplicationScale(2);
		HYP_CHECK(Gui.Scale(10) == 10);
		const auto Large = Frame(1);
		HYP_CHECK(Gui.Scale(10) == 20 && Large.FontAtlas != Original.FontAtlas);
		HYP_CHECK(Bounds.W - Bounds.Y >= (OriginalBounds.W - OriginalBounds.Y) * 1.8f);
		HYP_CHECK(Bounds.Z - Bounds.X >= (OriginalBounds.Z - OriginalBounds.X) * 1.8f);
		const auto LargeBounds = Bounds;
		const auto Dense = Frame(2);
		HYP_CHECK(Dense.FramebufferScale.X == 2 && Dense.FontAtlas != Large.FontAtlas);
		HYP_CHECK(std::abs((Bounds.W - Bounds.Y) - (LargeBounds.W - LargeBounds.Y)) <= 1);
		HYP_CHECK(Dense.FontAtlas->Width * Dense.FontAtlas->Height > Large.FontAtlas->Width * Large.FontAtlas->Height);
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (Bounds.X + Bounds.Z) / 2;
		Move.Y = (Bounds.Y + Bounds.W) / 2;
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.Button = 0;
		Button.bDown = true;
		Frame(2, std::array{Move, Button});
		Button.bDown = false;
		Frame(2, std::array{Button});
		HYP_CHECK(bChecked == (Iteration % 2 == 0));
		Gui.SetApplicationScale(1);
		const auto Restored = Frame(1);
		HYP_CHECK(Bounds.X == OriginalBounds.X && Bounds.Y == OriginalBounds.Y);
		HYP_CHECK(Bounds.Z == OriginalBounds.Z && Bounds.W == OriginalBounds.W);
		HYP_CHECK(Restored.FontAtlas->Rgba == Original.FontAtlas->Rgba);
	}
}
