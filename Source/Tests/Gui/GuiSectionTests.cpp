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

struct FTreeInput
{
	FGui Gui;
	FVec4 Row;
	FVec4 Toggle;
	FVec4 LeafToggle;
	unsigned Ancestors{};
	bool bOpen{};
	bool bLeafVisible{};

	void Frame(std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({1200, 800}, {1200, 800}, 1.f / 60, InEvents);
		Gui.BeginPanel("Tree", {0, 0}, {1000, 700});
		if (Gui.BeginTable("Objects", "Item Label", "Type"))
		{
			bool bClicked{};
			for (unsigned Index = 0; Index < Ancestors; ++Index)
			{
				Gui.NextRow();
				Gui.NextColumn();
				HYP_CHECK(Gui.TreeItem(std::to_string(Index).c_str(), "Ancestor", false, false, bClicked));
			}
			Gui.NextRow();
			Gui.NextColumn();
			bOpen = Gui.TreeItem("parent", "Parent", false, false, bClicked, true, &Toggle);
			Row = Gui.LastItemBounds();
			bLeafVisible = bOpen;
			if (bOpen)
			{
				Gui.NextRow();
				Gui.NextColumn();
				if (Gui.TreeItem("leaf", "Leaf", true, false, bClicked, true, &LeafToggle))
				{
					Gui.EndTree();
				}
				Gui.EndTree();
			}
			for (unsigned Index = 0; Index < Ancestors; ++Index)
			{
				Gui.EndTree();
			}
			Gui.EndTable();
		}
		Gui.EndPanel();
		Gui.Render();
	}

	void ClickToggle()
	{
		FInputEvent Event;
		Event.Type = EEventType::MouseMove;
		Event.X = (Toggle.X + Toggle.Z) / 2;
		Event.Y = (Toggle.Y + Toggle.W) / 2;
		Frame(std::span(&Event, 1));
		Event.Type = EEventType::MouseButton;
		Event.bDown = true;
		Frame(std::span(&Event, 1));
		Event.bDown = false;
		Frame(std::span(&Event, 1));
	}
};

void CheckTreeControls()
{
	for (const float Scale : {1.f, 2.f})
	{
		for (const unsigned Ancestors : {0u, 1u, 3u})
		{
			FTreeInput Input;
			Input.Ancestors = Ancestors;
			Input.Gui.UseEditorStyle();
			Input.Gui.SetApplicationScale(Scale);
			Input.Frame();
			Input.Frame();
			HYP_CHECK(Input.bOpen && Input.bLeafVisible);
			HYP_CHECK(Input.LeafToggle.X == 0 && Input.LeafToggle.Y == 0 && Input.LeafToggle.Z == 0 &&
			          Input.LeafToggle.W == 0);
			HYP_CHECK(Input.Toggle.X >= Input.Row.X && Input.Toggle.Z < Input.Row.Z);
			if (Ancestors)
			{
				HYP_CHECK(Input.Toggle.X > Input.Row.X + Input.Gui.Scale(8));
			}
			Input.ClickToggle();
			HYP_CHECK(!Input.bOpen && !Input.bLeafVisible);
			Input.ClickToggle();
			HYP_CHECK(Input.bOpen && Input.bLeafVisible);
		}
	}
}
} // namespace

void CheckSectionControls()
{
	CheckTreeControls();
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
