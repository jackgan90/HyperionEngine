#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

struct FMixedNumberInput
{
	FGui Gui;
	FVec3 Primary{3, 4, 5};
	FVec3 Secondary{7, 8, 9};
	FVec4 InputBounds;
	FVec4 OtherBounds;
	unsigned Changes{};
	bool bVector{};

	explicit FMixedNumberInput(bool bInVector) : bVector(bInVector)
	{
		Gui.FontImage();
		Frame({});
		Frame({});
	}

	void Frame(std::span<const FInputEvent> InEvents)
	{
		Gui.BeginFrame({900, 800}, {900, 800}, 1.f / 60, InEvents);
		Gui.BeginPanel("Mixed number", {0, 0}, {850, 700});
		Gui.BeginLiveEdit();
		std::array<bool, 3> Edited{};
		if (bVector)
		{
			std::array<FVec4, 3> Bounds;
			Gui.InputVectorRow("Position", Primary, {}, {}, Bounds,
			                   {Primary.X != Secondary.X, Primary.Y != Secondary.Y, Primary.Z != Secondary.Z}, &Edited);
			InputBounds = Bounds[0];
		}
		else
		{
			auto Value = WriteValue(double(Primary.X));
			Gui.BeginPropertyRow("Value");
			Edited[0] = Gui.EditMixedScalar(Value, {.Kind = ERecordValueKind::Number}, {}, Primary.X != Secondary.X);
			InputBounds = Gui.LastItemBounds();
			Gui.EndPropertyRow();
			Primary.X = float(ReadValue<double>(Value));
		}
		if (Edited[0])
		{
			Secondary.X = Primary.X;
			++Changes;
		}
		HYP_CHECK(!Edited[1] && !Edited[2]);
		Gui.EndLiveEdit();
		Gui.Button("Other");
		OtherBounds = Gui.LastItemBounds();
		Gui.EndPanel();
		Gui.Render();
	}

	void Click(FVec4 InBounds)
	{
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = (InBounds.X + InBounds.Z) / 2;
		Move.Y = (InBounds.Y + InBounds.W) / 2;
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = true;
		Frame(std::array{Move, Button});
		Button.bDown = false;
		Frame(std::span(&Button, 1));
	}

	void Type(const char* InText)
	{
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Modifiers = 1;
		Frame(std::span(&Key, 1));
		Click(InputBounds);
		Key.Key = EKey::A;
		Key.bDown = true;
		Frame(std::span(&Key, 1));
		Key.Modifiers = 0;
		Key.bDown = false;
		FInputEvent Text;
		Text.Type = EEventType::Text;
		Text.Text = InText;
		Frame(std::array{Key, Text});
	}
};

void CheckMixedNumberInput(bool bInVector, const char* InText, bool bInEnter)
{
	FMixedNumberInput Input(bInVector);
	Input.Type(InText);
	Input.Frame({});
	const bool bTypedValue = std::string_view(InText) == "3.0";
	HYP_CHECK(Input.Secondary.X == (bTypedValue ? 3 : 7));
	HYP_CHECK(Input.Changes == (bTypedValue ? 1u : 0u));
	if (bInEnter)
	{
		FInputEvent Key;
		Key.Type = EEventType::Key;
		Key.Key = EKey::Enter;
		Key.bDown = true;
		Input.Frame(std::span(&Key, 1));
	}
	else
	{
		Input.Click(Input.OtherBounds);
	}
	Input.Frame({});
	const bool bAssigned = bTypedValue || (bInEnter && std::string_view(InText).empty());
	HYP_CHECK(Input.Secondary.X == (bAssigned ? 3 : 7));
	HYP_CHECK(Input.Changes == (bAssigned ? 1u : 0u));
	HYP_CHECK(Input.Primary.X == 3 && Input.Primary.Y == 4 && Input.Primary.Z == 5);
	HYP_CHECK(Input.Secondary.Y == 8 && Input.Secondary.Z == 9);
}

void CheckMixedVectorLayout()
{
	FGui Gui;
	Gui.FontImage();
	FVec3 Value{1, 2, 3};
	std::array<FVec4, 3> Bounds;
	std::array<bool, 3> Edited;
	const auto Frame = [&](const std::array<bool, 3>& InMixed)
	{
		Gui.BeginFrame({800, 800}, {800, 800}, 1.f / 60, {});
		Gui.BeginPanel("Mixed vector", {0, 0}, {300, 700});
		Gui.InputVectorRow("Position", Value, {}, {}, Bounds, InMixed, &Edited);
		Gui.EndPanel();
		Gui.Render();
	};
	Frame({true, false, true});
	Frame({true, false, true});
	const auto MixedBounds = Bounds;
	HYP_CHECK(Bounds[0].Z - Bounds[0].X > 100 && Bounds[1].Y > Bounds[0].Y && Bounds[2].Y > Bounds[1].Y);
	Frame({});
	for (unsigned Axis = 0; Axis < 3; ++Axis)
	{
		HYP_CHECK(Bounds[Axis].X == MixedBounds[Axis].X && Bounds[Axis].Y == MixedBounds[Axis].Y);
		HYP_CHECK(!Edited[Axis]);
	}
}

void CheckEmptyMixedString()
{
	FGui Gui;
	Gui.FontImage();
	FArchiveNode Value = WriteValue(std::string{});
	FVec4 Bounds;
	bool bChanged{};
	const auto Frame = [&](std::span<const FInputEvent> InEvents)
	{
		Gui.BeginFrame({800, 800}, {800, 800}, 1.f / 60, InEvents);
		Gui.BeginPanel("Mixed string", {0, 0}, {500, 700});
		Gui.BeginLiveEdit();
		Gui.BeginPropertyRow("Value");
		bChanged |= Gui.EditMixedScalar(Value, {.Kind = ERecordValueKind::String}, {}, true);
		Bounds = Gui.LastItemBounds();
		Gui.EndPropertyRow();
		Gui.EndLiveEdit();
		Gui.EndPanel();
		Gui.Render();
	};
	Frame({});
	Frame({});
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = (Bounds.X + Bounds.Z) / 2;
	Move.Y = (Bounds.Y + Bounds.W) / 2;
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.bDown = true;
	Frame(std::array{Move, Button});
	Button.bDown = false;
	Frame(std::span(&Button, 1));
	HYP_CHECK(!bChanged);
	FInputEvent Enter;
	Enter.Type = EEventType::Key;
	Enter.Key = EKey::Enter;
	Enter.bDown = true;
	Frame(std::span(&Enter, 1));
	HYP_CHECK(bChanged && ReadValue<std::string>(Value).empty());
}
} // namespace

void CheckMixedControls()
{
	CheckMixedVectorLayout();
	CheckEmptyMixedString();
	for (const bool bVector : {false, true})
	{
		for (const bool bEnter : {false, true})
		{
			for (const char* Text : {"3.0", "", "-"})
			{
				CheckMixedNumberInput(bVector, Text, bEnter);
			}
		}
	}
}
