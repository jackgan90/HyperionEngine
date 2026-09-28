#include "Hyperion/Gui/Gui.h"
#include "Support/TestSupport.h"

namespace Hyperion
{
struct FVisibilityChild
{
	bool bShow{};
	double Value = 7;
};

template<> const FRecordDescriptor& RecordType<FVisibilityChild>()
{
	static const auto Type = MakeRecord<FVisibilityChild>(
	    "test.visibilitychild",
	    {Member("show", &FVisibilityChild::bShow, Inspect("Show")),
	     Member("value", &FVisibilityChild::Value,
	            {.Inspector = FPropertyPresentation{.Label = "Conditional value",
	                                                .VisibleWhen = FPropertyCondition{"show", {WriteValue(true)}}}})});
	return Type;
}
} // namespace Hyperion

namespace
{
using namespace Hyperion;

struct FVisibilityParent
{
	FVisibilityChild Child;
};

float InspectionHeight(bool bInSelection, bool bInShow, bool bInMixed)
{
	const auto Type = MakeRecord<FVisibilityParent>("test.visibilityparent",
	                                                {Member("child", &FVisibilityParent::Child, Inspect("Child"))});
	FVisibilityParent First{{bInShow}};
	FVisibilityParent Second{{bInMixed ? !bInShow : bInShow}};
	const std::array<const void*, 2> Values{&First, &Second};
	FRecordDraft Single(Type, &First);
	FRecordSelectionDraft Selection(Type, Values);
	FGui Gui;
	Gui.FontImage();
	FVec4 Header;
	float Height{};
	const auto Frame = [&](std::span<const FInputEvent> InEvents = {})
	{
		Gui.BeginFrame({800, 600}, {800, 600}, 1.f / 60, InEvents);
		Gui.BeginPanel("Conditional inspection", {0, 0}, {700, 500});
		if (bInSelection)
		{
			Gui.EditRecord(Selection, "nested");
		}
		else
		{
			Gui.EditRecord(Single, "nested");
		}
		Header = Gui.LastItemBounds();
		Gui.Button("After properties");
		Height = Gui.LastItemBounds().Y;
		Gui.EndPanel();
		Gui.Render();
	};
	Frame();
	Frame();
	// The root section starts collapsed; open the actual nested record with pointer input.
	FInputEvent Move;
	Move.Type = EEventType::MouseMove;
	Move.X = Header.X + 10;
	Move.Y = (Header.Y + Header.W) / 2;
	Frame(std::span(&Move, 1));
	FInputEvent Button;
	Button.Type = EEventType::MouseButton;
	Button.bDown = true;
	Frame(std::span(&Button, 1));
	Button.bDown = false;
	Frame(std::span(&Button, 1));
	Frame();
	HYP_CHECK(First.Child.Value == 7 && Second.Child.Value == 7);
	return Height;
}
} // namespace

void CheckVisibilityControls()
{
	for (const bool bSelection : {false, true})
	{
		const float Hidden = InspectionHeight(bSelection, false, false);
		const float Visible = InspectionHeight(bSelection, true, false);
		HYP_CHECK(Visible > Hidden + 15);
		if (bSelection)
		{
			HYP_CHECK(InspectionHeight(true, true, true) == Hidden);
		}
	}
}
