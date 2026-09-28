#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"

namespace
{
using namespace Hyperion;

struct FSectionInput
{
	FGui Gui;
	FVec4 Header;
	FVec4 RequiredHeader;
	bool bDefaultOpen{};
	bool bOpen{};
	bool bRequiredOpen{};
	unsigned CloseCount{};
	float Width{};

	void Frame(std::span<const FInputEvent> InEvents)
	{
		Gui.BeginFrame({1200, 800}, {1200, 800}, 1.f / 60, InEvents);
		Gui.BeginPanel("Components", {0, 0}, {Width, 700});
		bool bClose = false;
		bOpen = Gui.Section("Model##object/model", bDefaultOpen, &bClose, "Remove Model component");
		Header = Gui.LastItemBounds();
		CloseCount += bClose ? 1 : 0;
		if (bOpen)
		{
			Gui.Indent();
			Gui.Text("Model asset");
			HYP_CHECK(Gui.LastItemBounds().X > Header.X);
			FVec3 Value{1, 2, 3};
			std::array<FVec4, 3> Bounds;
			Gui.InputVectorRow("Position", Value, {}, {}, Bounds);
			for (const auto& Axis : Bounds)
			{
				HYP_CHECK(Axis.X > Header.X && Axis.Z <= Header.Z);
			}
			Gui.Unindent();
		}
		bRequiredOpen = Gui.Section("Transform##object/transform");
		RequiredHeader = Gui.LastItemBounds();
		HYP_CHECK(RequiredHeader.X == Header.X);
		Gui.EndPanel();
		Gui.Render();
	}

	void Click(float InX, float InY)
	{
		FInputEvent Move;
		Move.Type = EEventType::MouseMove;
		Move.X = InX;
		Move.Y = InY;
		Frame(std::span(&Move, 1));
		FInputEvent Button;
		Button.Type = EEventType::MouseButton;
		Button.bDown = true;
		Frame(std::span(&Button, 1));
		Button.bDown = false;
		Frame(std::span(&Button, 1));
	}
};
} // namespace

void CheckSectionControls()
{
	for (const float Scale : {1.f, 2.f})
	{
		for (const float Width : {260.f, 560.f})
		{
			for (const bool bDefaultOpen : {false, true})
			{
				FSectionInput Input;
				Input.Width = Width * Scale;
				Input.bDefaultOpen = bDefaultOpen;
				Input.Gui.UseEditorStyle();
				Input.Gui.SetApplicationScale(Scale);
				Input.Frame({});
				Input.Frame({});
				const float HalfHeight = (Input.Header.W - Input.Header.Y) / 2;
				Input.Click(Input.Header.Z - HalfHeight, Input.Header.Y + HalfHeight);
				HYP_CHECK(Input.CloseCount == 1);
				HYP_CHECK(Input.bOpen == bDefaultOpen && Input.bRequiredOpen);
				// Ignoring the action (e.g. a rejected domain edit) preserves the header and expansion.
				Input.Frame({});
				HYP_CHECK(Input.bOpen == bDefaultOpen && Input.CloseCount == 1);
				Input.Click(Input.Header.X + HalfHeight, Input.Header.Y + HalfHeight);
				HYP_CHECK(Input.bOpen != bDefaultOpen && Input.CloseCount == 1);
				Input.Click(Input.RequiredHeader.Z - HalfHeight, Input.RequiredHeader.Y + HalfHeight);
				HYP_CHECK(!Input.bRequiredOpen && Input.CloseCount == 1);
			}
		}
	}
}
